// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class UMaterialInterface;
struct FCSCameraFadeMaterialData;

/**
 * 원본 머티리얼 그래프에서 페이드 머티리얼에 넣을 값을 정확히 읽어낸다. 에디터 빌드에만 존재한다.
 *
 * 지원 범위 (이 안이면 원본과 같은 색·질감이 보장된다):
 *  - Surface 도메인, Opaque, MaterialAttributes 미사용
 *  - BaseColor  : TextureSample(RGB) [× 상수/벡터 파라미터] | 상수 | 벡터 파라미터
 *  - Normal     : TextureSample(RGB) | 미연결
 *  - Roughness  : TextureSample 한 채널 | 상수 | 스칼라 파라미터 | 미연결
 *  - Metallic   : 위와 같음
 *  - 텍스처 UV  : 미연결(UV0) 또는 TextureCoordinate(인덱스 0, 타일링). 모든 텍스처가 같은 타일링이어야 한다.
 *  - Reroute / Named Reroute 는 투과한다. 인스턴스의 파라미터 오버라이드는 최종값으로 읽는다.
 *
 * 범위 밖이면 false 를 돌려주고 OutWhy 에 어느 노드 때문인지 쓴다. 근사값을 만들지 않는다.
 * 틀린 색으로 페이드하느니 그 머티리얼은 페이드하지 않는 편이 낫다.
 */
struct FCSCameraFadeMaterialExtractor
{
	static bool Extract(UMaterialInterface* Source, FCSCameraFadeMaterialData& Out, FString& OutWhy);
};

#endif // WITH_EDITOR
