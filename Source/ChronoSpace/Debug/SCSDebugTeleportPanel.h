// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class ACSPlayerController;
class SVerticalBox;

/**
 * 체크포인트 순간이동 디버그 패널 (Slate).
 *
 * ACSPlayerController 가 상단 숫자열 0 키로 열고 닫는다.
 * 열릴 때마다 월드의 ACSCheckPoint 를 다시 훑으므로 좌표를 코드에 박아둘 필요가 없다.
 * 이동 자체는 서버 권한이라 PC 의 Server RPC 로 넘긴다.
 */
class CHRONOSPACE_API SCSDebugTeleportPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCSDebugTeleportPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ACSPlayerController>, OwningPlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** 목록 한 줄이 들고 있는 이동 목표 */
	struct FEntry
	{
		FString  Label;
		FVector  Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		bool     bFromRespawnPoint = false;
	};

	/** 월드에서 체크포인트를 다시 수집하고 목록 위젯을 새로 만든다 */
	void RebuildEntries();

	FReply OnEntryClicked(int32 EntryIndex);
	FReply OnRefreshClicked();
	FReply OnCloseClicked();

	ECheckBoxState IsTeleportAllChecked() const;
	void OnTeleportAllChanged(ECheckBoxState NewState);

	FText GetStatusText() const;

	TWeakObjectPtr<ACSPlayerController> OwningPC;
	TArray<FEntry> Entries;
	TSharedPtr<SVerticalBox> EntryBox;

	/** 체크포인트는 보통 둘이 같이 진행하는 지점이라 기본값은 전원 이동 */
	bool bTeleportAllPlayers = true;

	FText StatusText;
};
