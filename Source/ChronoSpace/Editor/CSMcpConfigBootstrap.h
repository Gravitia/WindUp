// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/**
 * 프로젝트 루트의 .mcp.json 에 unreal-mcp 엔트리가 있도록 보장한다.
 *
 * 이 파일은 git 추적 대상이 아니다. NarshaMCP 같은 도구가 자기 엔트리를
 * 머신별 절대경로로 써넣기 때문에 공유하면 사람마다 틀린 값이 된다.
 * 대신 에디터가 켜질 때마다 여기서 unreal-mcp 엔트리만 채워 넣는다.
 */
class FCSMcpConfigBootstrap
{
public:
	/** 에디터 시작 시 1회 호출. 이미 엔트리가 있으면 아무것도 하지 않는다. */
	static void EnsureProjectMcpConfig();
};

#endif // WITH_EDITOR
