// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCS, Log, All);

/**
 * 게임 모듈. 에디터에서만 .mcp.json 부트스트랩을 돌린다.
 * 그 외에는 FDefaultGameModuleImpl 과 동작이 같다.
 */
class FChronoSpaceModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
};
