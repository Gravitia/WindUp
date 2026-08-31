// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class ACSPlayerController;
class SVerticalBox;

/**
 * 디버그 패널 (Slate).
 *
 * ACSPlayerController 가 상단 숫자열 0 키로 열고 닫는다.
 *  - 체크포인트 순간이동: 열릴 때마다 월드의 ACSCheckPoint 를 다시 훑으므로 좌표를 코드에 박아둘 필요가 없다.
 *  - 플레이어 소환: 한 플레이어를 다른 플레이어 옆으로 옮긴다.
 *  - 캐릭터 스왑: 두 플레이어가 조작 중인 몸을 서로 맞바꾼다.
 *  - 분할 화면 좌우 고정: 화면 절반을 몸에 묶는다 (P1 왼쪽 / P2 오른쪽).
 *  - 스테이지 트래블: UCSStageDataSettings 테이블의 스테이지로 ServerTravel 한다 (CSStagePortal 과 같은 데이터).
 * 이동·트래블·스왑 자체는 서버 권한이라 PC 의 Server RPC 로 넘긴다.
 * 좌우 고정만 로컬 화면 상태다 (프로세스 전역 CVar).
 *
 * 캐릭터 스왑의 전제(PIE 를 리슨 서버 2인으로 실행)와 한계(PlayerState 는 컨트롤러를 따라간다)는
 * CSPlayerController.h 의 "Debug — 다른 플레이어와 조작 캐릭터 맞바꾸기" 주석에 전부 적혀 있다.
 */
class CHRONOSPACE_API SCSDebugPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCSDebugPanel) {}
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

	/** 스테이지 목록 한 줄. UCSStageDataSettings 에서 만든다. */
	struct FStageEntry
	{
		int32   Chapter = 0;
		int32   Stage = 0;
		FString Label;
		FString LevelName;
		bool    bMapped = false;
	};

	/** 월드에서 체크포인트를 다시 수집하고 목록 위젯을 새로 만든다 */
	void RebuildEntries();

	/** 스테이지 데이터 테이블을 읽어 스테이지 목록 위젯을 새로 만든다 */
	void RebuildStageEntries();

	FReply OnEntryClicked(int32 EntryIndex);
	FReply OnStageClicked(int32 StageIndex);
	FReply OnSummonClicked(int32 MovingPlayerIndex, int32 AnchorPlayerIndex);
	FReply OnRefreshClicked();
	FReply OnCloseClicked();

	ECheckBoxState IsTeleportAllChecked() const;
	void OnTeleportAllChanged(ECheckBoxState NewState);

	ECheckBoxState IsShowCollisionChecked() const;
	void OnShowCollisionChanged(ECheckBoxState NewState);

	// --- 캐릭터 스왑 / 분할 화면 좌우 고정 ---
	// 라벨과 활성화 상태는 매 프레임 폴링되는 Slate 어트리뷰트로 묶는다. PC 의 복제 상태가
	// 진실이고 위젯은 그걸 비추기만 하므로, 클라이언트에서 눌러도 표시가 어긋나지 않는다.
	FReply OnSwapCharacterClicked();
	FText  GetSwapButtonText() const;
	FText  GetSwapCaptionText() const;
	bool   IsSwapButtonEnabled() const;

	ECheckBoxState IsFixedSplitSideChecked() const;
	void OnFixedSplitSideChanged(ECheckBoxState NewState);

	FText GetStatusText() const;

	TWeakObjectPtr<ACSPlayerController> OwningPC;
	TArray<FEntry> Entries;
	TSharedPtr<SVerticalBox> EntryBox;

	TArray<FStageEntry> StageEntries;
	TSharedPtr<SVerticalBox> StageBox;

	/** 체크포인트는 보통 둘이 같이 진행하는 지점이라 기본값은 전원 이동 */
	bool bTeleportAllPlayers = true;

	FText StatusText;
};
