// Fill out your copyright notice in the Description page of Project Settings.

#include "ChronoSpace.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor/CSMcpConfigBootstrap.h"
#endif

DEFINE_LOG_CATEGORY(LogCS);

void FChronoSpaceModule::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();

#if WITH_EDITOR
	// .mcp.json 은 커밋하지 않는다. 자세한 이유는 CSMcpConfigBootstrap.cpp 머리말 참고.
	FCSMcpConfigBootstrap::EnsureProjectMcpConfig();
#endif
}

IMPLEMENT_PRIMARY_GAME_MODULE( FChronoSpaceModule, ChronoSpace, "ChronoSpace" );
