// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSCameraOcclusionFadeSubsystem.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "Character/CSCharacterPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Common/CSCameraFadeMaterials.h"
#include "DataAsset/CSCameraFadeMaterialTable.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ChronoSpace.h"

namespace CSCameraOcclusionFade
{
	// 캡슐 위 샘플 격자: 세로 4단 × 가로 3열 = 12 광선
	static const float RowFractions[] = { -0.6f, -0.2f, 0.2f, 0.6f };
	static const float ColFractions[] = { -0.7f, 0.f, 0.7f };
}

bool UCSCameraOcclusionFadeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) return false;

	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UCSCameraOcclusionFadeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCSCameraOcclusionFadeSubsystem::Deinitialize()
{
	RestoreAll();
	Targets.Reset();
	Super::Deinitialize();
}

void UCSCameraOcclusionFadeSubsystem::RegisterTarget(AActor* Actor)
{
	if (IsValid(Actor))
	{
		Targets.AddUnique(Actor);
	}
}

void UCSCameraOcclusionFadeSubsystem::UnregisterTarget(AActor* Actor)
{
	if (!Actor) return;

	Targets.Remove(Actor);

	if (const FCSCameraOcclusionFadeEntry* Entry = Faded.Find(Actor))
	{
		RestoreEntry(*Entry);
		Faded.Remove(Actor);
	}
}

TStatId UCSCameraOcclusionFadeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCSCameraOcclusionFadeSubsystem, STATGROUP_Tickables);
}

void UCSCameraOcclusionFadeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;

	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	if (!Settings || !Settings->bEnabled)
	{
		RestoreAll();
		return;
	}

	// 1) 목표값 초기화. 이번 틱에 안 걸리면 0 으로 남아 페이드아웃된다.
	for (auto& Pair : Faded)
	{
		Pair.Value.TargetFade = 0.f;
	}

	// 2) 모든 뷰에 대해 막힌 비율 판정
	TArray<FViewPair> Views;
	CollectViews(Views);

	if (Views.Num() > 0)
	{
		// 파괴된 액터는 GC 가 null 로 만든다. 여기서 걷어내 UpdateTargets 가 null 을 보지 않게 한다.
		Targets.RemoveAll([](const TObjectPtr<AActor>& A) { return !IsValid(A); });

		for (const FViewPair& View : Views)
		{
			UpdateTargets(View, Targets);
		}
	}

	// 3) 시간 보간 + 적용 + 정리
	for (auto It = Faded.CreateIterator(); It; ++It)
	{
		AActor* Actor = It.Key();
		FCSCameraOcclusionFadeEntry& Entry = It.Value();

		if (!IsValid(Actor))
		{
			It.RemoveCurrent();
			continue;
		}

		const float Speed = (Entry.TargetFade > Entry.CurrentFade) ? Settings->FadeInSpeed : Settings->FadeOutSpeed;
		Entry.CurrentFade = FMath::FInterpConstantTo(Entry.CurrentFade, Entry.TargetFade, DeltaTime, Speed);

		if (Entry.CurrentFade <= KINDA_SMALL_NUMBER && Entry.TargetFade <= KINDA_SMALL_NUMBER)
		{
			RestoreEntry(Entry);
			It.RemoveCurrent();
			continue;
		}

		ApplyFadeAmount(Entry, Entry.CurrentFade);
	}
}

void UCSCameraOcclusionFadeSubsystem::CollectViews(TArray<FViewPair>& OutViews) const
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 메인 뷰: 이 머신의 로컬 플레이어 카메라 → 조작 중인 폰
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) continue;

		APawn* Pawn = PC->GetPawn();
		if (!IsValid(Pawn)) continue;

		OutViews.Add({ PC->PlayerCameraManager->GetCameraLocation(), Pawn });
	}

	// 스플릿 보조 뷰: 원격 플레이어의 카메라 → 원격 캐릭터. 화면에 그려질 때만.
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UCSSplitScreenSubsystem* Split = GI->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			FVector SecondaryCam;
			ACSCharacterPlayer* SecondaryTarget = nullptr;
			if (Split->GetSecondaryViewForOcclusion(SecondaryCam, SecondaryTarget))
			{
				OutViews.Add({ SecondaryCam, SecondaryTarget });
			}
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::BuildSamplePoints(const FViewPair& View, TArray<FVector>& OutPoints)
{
	const AActor* Target = View.Target;
	const FVector Center = Target->GetActorLocation();

	float HalfHeight = 88.f;
	float Radius = 34.f;
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			Radius = Capsule->GetScaledCapsuleRadius();
		}
	}

	// 카메라에서 본 화면 가로 방향. 캡슐을 화면에 보이는 폭만큼 가로로 훑는다.
	FVector ToCamera = (View.CameraLocation - Center);
	ToCamera.Z = 0.f;
	const FVector Right = ToCamera.IsNearlyZero()
		? FVector::RightVector
		: FVector::CrossProduct(FVector::UpVector, ToCamera.GetSafeNormal());

	OutPoints.Reset();
	for (float Row : CSCameraOcclusionFade::RowFractions)
	{
		for (float Col : CSCameraOcclusionFade::ColFractions)
		{
			OutPoints.Add(Center + FVector(0.f, 0.f, HalfHeight * Row) + Right * (Radius * Col));
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::UpdateTargets(const FViewPair& View, const TArray<TObjectPtr<AActor>>& Candidates)
{
	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	const float ProbeRadius = Settings ? Settings->ProbeRadius : 12.f;
	const FCollisionShape Probe = FCollisionShape::MakeSphere(ProbeRadius);

	TArray<FVector> Points;
	BuildSamplePoints(View, Points);
	if (Points.Num() == 0) return;

	// 전체 샘플을 감싸는 상자. 액터 바운드와 겹치지 않으면 스윕조차 하지 않는다.
	FBox RayBounds(ForceInit);
	RayBounds += View.CameraLocation;
	for (const FVector& P : Points) RayBounds += P;

	TInlineComponentArray<UStaticMeshComponent*> Meshes;

	for (const TObjectPtr<AActor>& Actor : Candidates)
	{
		if (!IsValid(Actor) || Actor == View.Target) continue;

		Meshes.Reset();
		Actor->GetComponents<UStaticMeshComponent>(Meshes);

		TArray<UStaticMeshComponent*, TInlineAllocator<8>> Testable;
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh) || !Mesh->IsVisible() || Mesh->bHiddenInGame) continue;
			if (Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
			if (!Mesh->Bounds.GetBox().ExpandBy(ProbeRadius).Intersect(RayBounds)) continue;
			Testable.Add(Mesh);
		}
		if (Testable.Num() == 0) continue;

		// 광선마다 "이 액터의 메시 중 하나라도 막는가" 를 센다. 액터 단위로 페이드해야
		// 기어 원통만 반투명해지고 위에 붙은 버튼은 불투명하게 남는 반쪽 현상이 없다.
		int32 Blocked = 0;
		for (const FVector& End : Points)
		{
			for (UStaticMeshComponent* Mesh : Testable)
			{
				FHitResult Hit;
				if (Mesh->SweepComponent(Hit, View.CameraLocation, End, FQuat::Identity, Probe))
				{
					++Blocked;
					break;
				}
			}
		}
		if (Blocked == 0) continue;

		const float Coverage = static_cast<float>(Blocked) / static_cast<float>(Points.Num());

		FCSCameraOcclusionFadeEntry* Entry = Faded.Find(Actor);
		if (!Entry)
		{
			BeginFade(Actor);
			Entry = Faded.Find(Actor);
			if (!Entry) continue;
		}
		// 뷰가 여럿이면 가장 많이 가리는 뷰 기준
		Entry->TargetFade = FMath::Max(Entry->TargetFade, Coverage);
	}
}

const FCSCameraFadeMaterialData* UCSCameraOcclusionFadeSubsystem::ResolveFadeData(UMaterialInterface* Original)
{
	if (!Original) return nullptr;

	FString Why;
	const FCSCameraFadeMaterialData* Data = FCSCameraFadeMaterials::Resolve(Original, &Why);
	if (!Data && !WarnedUnsupported.Contains(Original))
	{
		WarnedUnsupported.Add(Original);
		UE_LOG(LogCS, Warning, TEXT("CameraOcclusionFade: %s 는 페이드하지 않는다 - %s"), *Original->GetPathName(), *Why);
	}
	return Data;
}

void UCSCameraOcclusionFadeSubsystem::BeginFade(AActor* Actor)
{
	UMaterialInterface* FadeMaterial = FCSCameraFadeMaterials::GetFadeMaterial();
	if (!FadeMaterial)
	{
		if (!bWarnedNoFadeMaterial)
		{
			bWarnedNoFadeMaterial = true;
			UE_LOG(LogCS, Warning, TEXT("CameraOcclusionFade: 페이드 머티리얼이 없다. 프로젝트 설정의 FadeMaterial 을 확인한다."));
		}
		return;
	}

	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	const float MinOpacity = Settings ? Settings->MinOpacity : 0.15f;

	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents<UStaticMeshComponent>(Meshes);

	// 1) 먼저 보이는 메시의 모든 슬롯을 읽는다. 하나라도 못 읽으면 MID 를 만들기 전에 끝낸다.
	//    (실패한 액터는 가려져 있는 동안 매 틱 여기 다시 오지만, 캐시·표 조회뿐이라 싸다.)
	struct FPendingMesh
	{
		UStaticMeshComponent* Mesh = nullptr;
		TArray<UMaterialInterface*, TInlineAllocator<4>> Originals;
		TArray<FCSCameraFadeMaterialData, TInlineAllocator<4>> Data;	// 빈 슬롯은 기본값, 아래 bHasData 로 구분
		TArray<bool, TInlineAllocator<4>> bHasData;
	};
	TArray<FPendingMesh, TInlineAllocator<8>> Pending;

	auto SkipActor = [&](UMaterialInterface* Original, const TCHAR* Reason)
	{
		if (WarnedActors.Contains(Actor)) return;
		WarnedActors.Add(Actor);
		UE_LOG(LogCS, Warning, TEXT("CameraOcclusionFade: %s 는 페이드하지 않는다 - %s 를 %s. 일부 슬롯만 투명해지는 걸 막으려 액터 전체를 그대로 둔다."),
			*Actor->GetName(), *Original->GetPathName(), Reason);
	};

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !Mesh->IsVisible() || Mesh->bHiddenInGame) continue;

		FPendingMesh& P = Pending.AddDefaulted_GetRef();
		P.Mesh = Mesh;

		const int32 NumSlots = Mesh->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			UMaterialInterface* Original = Mesh->GetMaterial(Slot);
			P.Originals.Add(Original);

			if (!Original)
			{
				// 빈 슬롯은 아무것도 그리지 않으므로 페이드할 것도 없다
				P.Data.AddDefaulted();
				P.bHasData.Add(false);
				continue;
			}

			// Resolve 가 돌려주는 포인터는 다음 Resolve 까지만 유효하므로 값으로 복사한다
			const FCSCameraFadeMaterialData* Data = ResolveFadeData(Original);
			if (!Data)
			{
				SkipActor(Original, TEXT("읽을 수 없다"));
				return;
			}
			P.Data.Add(*Data);
			P.bHasData.Add(true);
		}
	}

	// 2) MID 를 만들고 값을 넣는다. 텍스처를 못 찾는 슬롯이 있으면 역시 액터 전체를 건너뛴다.
	TArray<TArray<UMaterialInstanceDynamic*, TInlineAllocator<4>>, TInlineAllocator<8>> MIDsPerMesh;
	MIDsPerMesh.SetNum(Pending.Num());
	for (int32 i = 0; i < Pending.Num(); ++i)
	{
		const FPendingMesh& P = Pending[i];
		for (int32 Slot = 0; Slot < P.Originals.Num(); ++Slot)
		{
			if (!P.bHasData[Slot])
			{
				MIDsPerMesh[i].Add(nullptr);
				continue;
			}
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FadeMaterial, this);
			if (!P.Data[Slot].ApplyTo(MID))
			{
				SkipActor(P.Originals[Slot], TEXT("의 텍스처를 찾지 못했다"));
				return;	// 아직 어떤 메시도 건드리지 않았다. 만든 MID 는 GC 가 치운다.
			}
			MID->SetScalarParameterValue(FCSCameraFadeParams::MinOpacity, MinOpacity);
			MID->SetScalarParameterValue(FCSCameraFadeParams::FadeAmount, 0.f);
			MIDsPerMesh[i].Add(MID);
		}
	}

	// 3) 전부 준비됐을 때만 실제로 교체한다
	FCSCameraOcclusionFadeEntry& Entry = Faded.Add(Actor);
	for (int32 i = 0; i < Pending.Num(); ++i)
	{
		const FPendingMesh& P = Pending[i];
		FCSMaterialSlots& Originals = Entry.Originals.Add(P.Mesh);
		FCSMaterialSlots& MIDs = Entry.FadeMIDs.Add(P.Mesh);
		for (int32 Slot = 0; Slot < P.Originals.Num(); ++Slot)
		{
			Originals.Materials.Add(P.Originals[Slot]);
			MIDs.Materials.Add(MIDsPerMesh[i][Slot]);
			if (MIDsPerMesh[i][Slot]) P.Mesh->SetMaterial(Slot, MIDsPerMesh[i][Slot]);
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::ApplyFadeAmount(FCSCameraOcclusionFadeEntry& Entry, float Amount)
{
	for (auto& Pair : Entry.FadeMIDs)
	{
		for (const TObjectPtr<UMaterialInterface>& Mat : Pair.Value.Materials)
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mat.Get()))
			{
				MID->SetScalarParameterValue(FCSCameraFadeParams::FadeAmount, Amount);
			}
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::RestoreEntry(const FCSCameraOcclusionFadeEntry& Entry)
{
	for (const auto& Pair : Entry.Originals)
	{
		UStaticMeshComponent* Mesh = Pair.Key;
		if (!IsValid(Mesh)) continue;

		const FCSMaterialSlots* MIDs = Entry.FadeMIDs.Find(Mesh);
		const TArray<TObjectPtr<UMaterialInterface>>& Originals = Pair.Value.Materials;

		for (int32 Slot = 0; Slot < Originals.Num(); ++Slot)
		{
			// 페이드 중에 게임플레이(버튼 점등 등)가 슬롯을 바꿨으면 그건 존중한다. 우리가 넣은 MID 만 되돌린다.
			const bool bStillOurs = MIDs && MIDs->Materials.IsValidIndex(Slot) && Mesh->GetMaterial(Slot) == MIDs->Materials[Slot];
			if (bStillOurs)
			{
				Mesh->SetMaterial(Slot, Originals[Slot]);
			}
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::RestoreAll()
{
	for (auto& Pair : Faded)
	{
		RestoreEntry(Pair.Value);
	}
	Faded.Empty();
}
