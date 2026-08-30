// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/SCSDebugTeleportPanel.h"

#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Player/CSPlayerController.h"
#include "Settings/CSStageDataSettings.h"

#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace CSDebugTeleportPanel
{
	/** 바닥에 박힌 채 나타나지 않도록 띄우는 높이. ACSGameMode::RespawnSinglePlayer 와 맞춘 값이다. */
	static constexpr float SpawnZOffset = 120.f;

	static const FLinearColor PanelBackground(0.015f, 0.017f, 0.025f, 0.93f);
	static const FLinearColor DividerColor(1.f, 1.f, 1.f, 0.14f);
	static const FLinearColor TitleColor(0.55f, 0.78f, 1.f);
	static const FLinearColor SubtleColor(0.55f, 0.60f, 0.70f);
	static const FLinearColor WarnColor(1.f, 0.62f, 0.30f);
	// FCoreStyle 의 Button 은 다크 테마다. 버튼 위 글자는 밝은 색이어야 읽힌다.
	static const FLinearColor RowTitleColor(0.93f, 0.95f, 1.f);
	static const FLinearColor RowDetailColor(0.60f, 0.66f, 0.78f);
}

void SCSDebugTeleportPanel::Construct(const FArguments& InArgs)
{
	OwningPC = InArgs._OwningPlayerController;

	// 패널이 뷰포트 전체를 덮지만 히트 테스트는 자식 위젯만 받게 한다.
	// 이게 없으면 패널 바깥 클릭까지 Slate 가 먹어서 게임 입력이 죽는다.
	SetVisibility(EVisibility::SelfHitTestInvisible);

	const FSlateBrush* FillBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	.Padding(FMargin(28.f, 28.f, 0.f, 0.f))
	[
		SNew(SBox)
		.WidthOverride(380.f)
		[
			SNew(SBorder)
			.BorderImage(FillBrush)
			.BorderBackgroundColor(CSDebugTeleportPanel::PanelBackground)
			.Padding(FMargin(12.f))
			[
				SNew(SVerticalBox)

				// --- 제목 줄 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						.ColorAndOpacity(CSDebugTeleportPanel::TitleColor)
						.Text(FText::FromString(TEXT("Debug Panel")))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get(), "Button")
						.ContentPadding(FMargin(8.f, 2.f))
						.OnClicked(this, &SCSDebugTeleportPanel::OnCloseClicked)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(CSDebugTeleportPanel::RowTitleColor)
							.Text(FText::FromString(TEXT("X")))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 8.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(CSDebugTeleportPanel::SubtleColor)
					.Text(FText::FromString(TEXT("Press 0 (top row) to close")))
				]

				// --- 구분선 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SBox)
					.HeightOverride(1.f)
					[
						SNew(SBorder)
						.BorderImage(FillBrush)
						.BorderBackgroundColor(CSDebugTeleportPanel::DividerColor)
					]
				]

				// --- 옵션 줄 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SCSDebugTeleportPanel::IsTeleportAllChecked)
						.OnCheckStateChanged(this, &SCSDebugTeleportPanel::OnTeleportAllChanged)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.ColorAndOpacity(FLinearColor::White)
						.Text(FText::FromString(TEXT("Move all players")))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get(), "Button")
						.ContentPadding(FMargin(10.f, 3.f))
						.OnClicked(this, &SCSDebugTeleportPanel::OnRefreshClicked)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(CSDebugTeleportPanel::RowTitleColor)
							.Text(FText::FromString(TEXT("Refresh")))
						]
					]
				]

				// --- 블루프린트 콜리전 표시 토글 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SCSDebugTeleportPanel::IsShowCollisionChecked)
						.OnCheckStateChanged(this, &SCSDebugTeleportPanel::OnShowCollisionChanged)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.ColorAndOpacity(FLinearColor::White)
							.Text(FText::FromString(TEXT("Show BP collision")))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.ColorAndOpacity(CSDebugTeleportPanel::SubtleColor)
							.Text(FText::FromString(TEXT("Box / Sphere / Capsule on Blueprint actors")))
						]
					]
				]

				// --- 플레이어 소환 ---
				// P1 = 호스트(서버 컨트롤러 0번), P2 = 클라이언트. 서버 RPC 쪽 인덱스와 같은 기준이다.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(0.f, 0.f, 3.f, 0.f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get(), "Button")
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(6.f, 4.f))
						.OnClicked(FOnClicked::CreateSP(this, &SCSDebugTeleportPanel::OnSummonClicked, 1, 0))
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(CSDebugTeleportPanel::RowTitleColor)
							.Text(FText::FromString(TEXT("Bring P2 to P1")))
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(3.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get(), "Button")
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(6.f, 4.f))
						.OnClicked(FOnClicked::CreateSP(this, &SCSDebugTeleportPanel::OnSummonClicked, 0, 1))
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(CSDebugTeleportPanel::RowTitleColor)
							.Text(FText::FromString(TEXT("Bring P1 to P2")))
						]
					]
				]

				// --- 체크포인트 목록 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(CSDebugTeleportPanel::TitleColor)
					.Text(FText::FromString(TEXT("CheckPoints")))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MaxDesiredHeight(240.f)
					[
						SNew(SScrollBox)

						+ SScrollBox::Slot()
						[
							SAssignNew(EntryBox, SVerticalBox)
						]
					]
				]

				// --- 스테이지 트래블 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 8.f, 0.f, 8.f)
				[
					SNew(SBox)
					.HeightOverride(1.f)
					[
						SNew(SBorder)
						.BorderImage(FillBrush)
						.BorderBackgroundColor(CSDebugTeleportPanel::DividerColor)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(CSDebugTeleportPanel::TitleColor)
					.Text(FText::FromString(TEXT("Stage Travel")))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(CSDebugTeleportPanel::SubtleColor)
					.Text(FText::FromString(TEXT("ServerTravel — moves everyone to the level")))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MaxDesiredHeight(220.f)
					[
						SNew(SScrollBox)

						+ SScrollBox::Slot()
						[
							SAssignNew(StageBox, SVerticalBox)
						]
					]
				]

				// --- 상태 줄 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(CSDebugTeleportPanel::SubtleColor)
					.Text(this, &SCSDebugTeleportPanel::GetStatusText)
					.AutoWrapText(true)
				]
			]
		]
	];

	RebuildEntries();
	RebuildStageEntries();
}

void SCSDebugTeleportPanel::RebuildEntries()
{
	Entries.Reset();

	ACSPlayerController* PC = OwningPC.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;

	if (World)
	{
		// 체크포인트는 서브레벨에 배치된 리플리케이트 액터라 클라이언트에서도 순회된다.
		for (TActorIterator<ACSCheckPoint> It(World); It; ++It)
		{
			ACSCheckPoint* CheckPoint = *It;
			if (!IsValid(CheckPoint))
			{
				continue;
			}

			FEntry Entry;

#if WITH_EDITOR
			Entry.Label = CheckPoint->GetActorLabel();
#else
			Entry.Label = CheckPoint->GetName();
#endif

			// 리스폰 포인트가 연결돼 있으면 그쪽이 실제 착지 지점이자 바라볼 방향이다.
			if (const ACSRespawnPoint* RespawnPoint = CheckPoint->ConnectedRespawnPoint)
			{
				Entry.Location = RespawnPoint->GetRespawnLocation();
				Entry.Rotation = RespawnPoint->GetRespawnRotation();
				Entry.bFromRespawnPoint = true;
			}
			else
			{
				Entry.Location = CheckPoint->GetActorLocation();
				Entry.Rotation = CheckPoint->GetActorRotation();
			}

			// 캡슐과 컨트롤 회전에 피치·롤이 섞이면 캐릭터가 눕는다.
			Entry.Rotation.Pitch = 0.f;
			Entry.Rotation.Roll = 0.f;
			Entry.Location.Z += CSDebugTeleportPanel::SpawnZOffset;

			Entries.Add(MoveTemp(Entry));
		}
	}

	// TActorIterator 순서는 보장이 없다. 매번 같은 줄 순서가 나오도록 라벨로 정렬한다.
	Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Label < B.Label; });

	if (!EntryBox.IsValid())
	{
		return;
	}

	EntryBox->ClearChildren();

	if (Entries.Num() == 0)
	{
		EntryBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 6.f)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.ColorAndOpacity(CSDebugTeleportPanel::WarnColor)
			.AutoWrapText(true)
			.Text(FText::FromString(TEXT("No CheckPoint in the loaded levels.")))
		];

		StatusText = FText::GetEmpty();
		return;
	}

	const APawn* LocalPawn = PC ? PC->GetPawn() : nullptr;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FEntry& Entry = Entries[Index];

		FString Detail = FString::Printf(TEXT("%.0f, %.0f, %.0f"),
			Entry.Location.X, Entry.Location.Y, Entry.Location.Z);

		if (LocalPawn)
		{
			const float DistanceMeters = FVector::Dist(LocalPawn->GetActorLocation(), Entry.Location) * 0.01f;
			Detail += FString::Printf(TEXT("    %.0fm"), DistanceMeters);
		}

		if (!Entry.bFromRespawnPoint)
		{
			Detail += TEXT("    [no respawn point]");
		}

		EntryBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get(), "Button")
			.HAlign(HAlign_Fill)
			.ContentPadding(FMargin(9.f, 5.f))
			.OnClicked(FOnClicked::CreateSP(this, &SCSDebugTeleportPanel::OnEntryClicked, Index))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(CSDebugTeleportPanel::RowTitleColor)
					.Text(FText::FromString(Entry.Label))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(CSDebugTeleportPanel::RowDetailColor)
					.Text(FText::FromString(Detail))
				]
			]
		];
	}

	StatusText = FText::FromString(FString::Printf(TEXT("%d checkpoint(s)"), Entries.Num()));
}

void SCSDebugTeleportPanel::RebuildStageEntries()
{
	StageEntries.Reset();

	// 개발자 설정(Config)이라 서버·클라이언트 어느 쪽에서 읽어도 같은 테이블이다.
	if (const UCSStageDataSettings* Data = UCSStageDataSettings::Get())
	{
		for (const FCSChapterDef& Chapter : Data->Chapters)
		{
			for (const FCSStageDef& StageDef : Chapter.Stages)
			{
				FStageEntry Entry;
				Entry.Chapter = Chapter.Chapter;
				Entry.Stage = StageDef.Stage;
				Entry.Label = Data->GetStageDisplayName(Chapter.Chapter, StageDef.Stage).ToString();

				const FString URL = Data->GetStageTravelURL(Chapter.Chapter, StageDef.Stage);
				Entry.bMapped = !URL.IsEmpty();
				if (Entry.bMapped)
				{
					Entry.LevelName = FPackageName::GetShortName(URL);
				}

				StageEntries.Add(MoveTemp(Entry));
			}
		}
	}

	if (!StageBox.IsValid())
	{
		return;
	}

	StageBox->ClearChildren();

	if (StageEntries.Num() == 0)
	{
		StageBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 6.f)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.ColorAndOpacity(CSDebugTeleportPanel::WarnColor)
			.AutoWrapText(true)
			.Text(FText::FromString(TEXT("No stages in Project Settings > ChronoSpace Stage Data.")))
		];
		return;
	}

	for (int32 Index = 0; Index < StageEntries.Num(); ++Index)
	{
		const FStageEntry& Entry = StageEntries[Index];

		FString Detail = FString::Printf(TEXT("C%d-S%d"), Entry.Chapter, Entry.Stage);
		Detail += Entry.bMapped
			? TEXT("    ") + Entry.LevelName
			: FString(TEXT("    [no level mapped]"));

		StageBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get(), "Button")
			.HAlign(HAlign_Fill)
			.ContentPadding(FMargin(9.f, 5.f))
			.IsEnabled(Entry.bMapped)
			.OnClicked(FOnClicked::CreateSP(this, &SCSDebugTeleportPanel::OnStageClicked, Index))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(CSDebugTeleportPanel::RowTitleColor)
					.Text(FText::FromString(Entry.Label))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(CSDebugTeleportPanel::RowDetailColor)
					.Text(FText::FromString(Detail))
				]
			]
		];
	}
}

FReply SCSDebugTeleportPanel::OnEntryClicked(int32 EntryIndex)
{
	ACSPlayerController* PC = OwningPC.Get();

	if (!PC || !Entries.IsValidIndex(EntryIndex))
	{
		StatusText = FText::FromString(TEXT("Teleport failed: no player controller."));
		return FReply::Handled();
	}

	const FEntry& Entry = Entries[EntryIndex];

	// 이동은 서버 권한이다. 클라이언트가 눌러도 서버가 실제로 옮긴다.
	PC->ServerDebugTeleport(Entry.Location, Entry.Rotation, bTeleportAllPlayers);

	StatusText = FText::FromString(FString::Printf(TEXT("-> %s%s"),
		*Entry.Label,
		bTeleportAllPlayers ? TEXT(" (all)") : TEXT(" (self)")));

	return FReply::Handled();
}

FReply SCSDebugTeleportPanel::OnStageClicked(int32 StageIndex)
{
	ACSPlayerController* PC = OwningPC.Get();

	if (!PC || !StageEntries.IsValidIndex(StageIndex))
	{
		StatusText = FText::FromString(TEXT("Travel failed: no player controller."));
		return FReply::Handled();
	}

	const FStageEntry& Entry = StageEntries[StageIndex];

	// 트래블은 서버 권한이다. 클라이언트가 눌러도 서버가 전원을 옮긴다.
	PC->ServerDebugTravelToStage(Entry.Chapter, Entry.Stage);

	StatusText = FText::FromString(FString::Printf(TEXT("ServerTravel -> %s ..."), *Entry.Label));
	return FReply::Handled();
}

FReply SCSDebugTeleportPanel::OnSummonClicked(int32 MovingPlayerIndex, int32 AnchorPlayerIndex)
{
	ACSPlayerController* PC = OwningPC.Get();

	if (!PC)
	{
		StatusText = FText::FromString(TEXT("Summon failed: no player controller."));
		return FReply::Handled();
	}

	// 소환도 서버 권한이다. 어느 클라이언트 패널에서 눌러도 결과는 같다.
	PC->ServerDebugSummonPlayer(MovingPlayerIndex, AnchorPlayerIndex);

	StatusText = FText::FromString(FString::Printf(TEXT("Summon P%d -> P%d"),
		MovingPlayerIndex + 1, AnchorPlayerIndex + 1));
	return FReply::Handled();
}

FReply SCSDebugTeleportPanel::OnRefreshClicked()
{
	RebuildEntries();
	RebuildStageEntries();
	return FReply::Handled();
}

FReply SCSDebugTeleportPanel::OnCloseClicked()
{
	if (ACSPlayerController* PC = OwningPC.Get())
	{
		PC->CloseDebugTeleportPanel();
	}

	return FReply::Handled();
}

ECheckBoxState SCSDebugTeleportPanel::IsTeleportAllChecked() const
{
	return bTeleportAllPlayers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCSDebugTeleportPanel::OnTeleportAllChanged(ECheckBoxState NewState)
{
	bTeleportAllPlayers = (NewState == ECheckBoxState::Checked);
}

ECheckBoxState SCSDebugTeleportPanel::IsShowCollisionChecked() const
{
	const ACSPlayerController* PC = OwningPC.Get();

	// 패널을 닫았다 열어도 실제 상태를 그대로 비추도록 PC 쪽 값을 읽는다.
	return (PC && PC->IsShowingBlueprintCollision()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCSDebugTeleportPanel::OnShowCollisionChanged(ECheckBoxState NewState)
{
	ACSPlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		return;
	}

	const bool bEnable = (NewState == ECheckBoxState::Checked);
	const int32 ShapeCount = PC->SetShowBlueprintCollision(bEnable);

	StatusText = bEnable
		? FText::FromString(FString::Printf(TEXT("collision on: %d shape(s)"), ShapeCount))
		: FText::FromString(TEXT("collision off"));
}

FText SCSDebugTeleportPanel::GetStatusText() const
{
	return StatusText;
}
