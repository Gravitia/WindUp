// Fill out your copyright notice in the Description page of Project Settings.

#include "ChronoSpace.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor/CSCameraFadeTableGenerator.h"
#endif

DEFINE_LOG_CATEGORY(LogCS);

class FChronoSpaceModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
		FCSCameraFadeTableGenerator::RegisterHooks();
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FChronoSpaceModule, ChronoSpace, "ChronoSpace" );
