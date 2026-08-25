// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/SCSDebugTeleportPanel.h"

#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Player/CSPlayerController.h"

#include "EngineUtils.h"
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
						.Text(FText::FromString(TEXT("CheckPoint Teleport")))
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

				// --- 체크포인트 목록 ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MaxDesiredHeight(420.f)
					[
						SNew(SScrollBox)

						+ SScrollBox::Slot()
						[
							SAssignNew(EntryBox, SVerticalBox)
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

FReply SCSDebugTeleportPanel::OnRefreshClicked()
{
	RebuildEntries();
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

FText SCSDebugTeleportPanel::GetStatusText() const
{
	return StatusText;
}
