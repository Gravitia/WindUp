// Fill out your copyright notice in the Description page of Project Settings.

#include "Editor/CSCameraFadeMaterialExtractor.h"

#if WITH_EDITOR

#include "DataAsset/CSCameraFadeMaterialTable.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Engine/Texture.h"

namespace
{
	/** 그래프의 한 지점을 따라간 결과 */
	struct FValue
	{
		enum class EKind : uint8 { Scalar, Color, Texture };
		EKind Kind = EKind::Scalar;

		float Scalar = 0.f;
		FLinearColor Color = FLinearColor::Black;

		UTexture* Texture = nullptr;
		/** 텍스처 샘플 출력 핀: 0 RGB, 1 R, 2 G, 3 B, 4 A, 5 RGBA */
		int32 Channel = 0;
		/** 텍스처에 곱해진 색 (Multiply 를 지나며 누적) */
		FLinearColor Tint = FLinearColor::White;
		FVector2D Tiling = FVector2D(1.f, 1.f);

		static FValue FromScalar(float V) { FValue R; R.Kind = EKind::Scalar; R.Scalar = V; return R; }
		static FValue FromColor(const FLinearColor& C) { FValue R; R.Kind = EKind::Color; R.Color = C; return R; }

		/** 색 노드의 출력 핀 인덱스(0 전체, 1~4 R/G/B/A)를 반영한다 */
		static FValue FromColorPin(const FLinearColor& C, int32 OutputIndex)
		{
			switch (OutputIndex)
			{
			case 1: return FromScalar(C.R);
			case 2: return FromScalar(C.G);
			case 3: return FromScalar(C.B);
			case 4: return FromScalar(C.A);
			default: return FromColor(C);
			}
		}

		FLinearColor AsColor() const
		{
			return Kind == EKind::Scalar ? FLinearColor(Scalar, Scalar, Scalar, 1.f) : Color;
		}
		bool IsWhiteTint() const { return Tint.Equals(FLinearColor::White, 1e-3f); }
	};

	FString NodeName(const UMaterialExpression* E)
	{
		return E ? E->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT("")) : TEXT("null");
	}

	bool Resolve(const UMaterialInterface* Source, UMaterialExpression* Expr, int32 OutputIndex, FValue& Out, FString& Why, int32 Depth);

	bool ResolveTextureOf(const UMaterialInterface* Source, UMaterialExpressionTextureSample* Sample, UTexture*& OutTex, FString& Why)
	{
		if (UMaterialExpression* ObjExpr = Sample->TextureObject.Expression)
		{
			if (auto* Param = Cast<UMaterialExpressionTextureObjectParameter>(ObjExpr))
			{
				UTexture* Tex = nullptr;
				if (Source->GetTextureParameterValue(FMaterialParameterInfo(Param->ParameterName), Tex) && Tex)
				{
					OutTex = Tex;
					return true;
				}
				OutTex = Param->Texture;
			}
			else if (auto* Obj = Cast<UMaterialExpressionTextureObject>(ObjExpr))
			{
				OutTex = Obj->Texture;
			}
			else
			{
				Why = FString::Printf(TEXT("TextureSample 의 Texture 입력이 %s 노드다"), *NodeName(ObjExpr));
				return false;
			}
		}
		else
		{
			OutTex = nullptr;
			if (auto* Param = Cast<UMaterialExpressionTextureSampleParameter>(Sample))
			{
				UTexture* Tex = nullptr;
				if (Source->GetTextureParameterValue(FMaterialParameterInfo(Param->ParameterName), Tex) && Tex)
				{
					OutTex = Tex;
				}
			}
			if (!OutTex) OutTex = Sample->Texture;
		}

		if (!OutTex)
		{
			Why = TEXT("TextureSample 에 텍스처가 없다");
			return false;
		}
		return true;
	}

	bool ResolveTilingOf(UMaterialExpressionTextureSample* Sample, FVector2D& OutTiling, FString& Why)
	{
		UMaterialExpression* UV = Sample->Coordinates.Expression;
		if (!UV)
		{
			if (Sample->ConstCoordinate != 0)
			{
				Why = FString::Printf(TEXT("TextureSample 이 UV%d 를 쓴다 (UV0 만 지원)"), Sample->ConstCoordinate);
				return false;
			}
			OutTiling = FVector2D(1.f, 1.f);
			return true;
		}
		// Reroute 를 지나 TexCoord 에 닿는 경우까지 허용
		for (int32 Guard = 0; Guard < 16 && UV; ++Guard)
		{
			if (auto* Reroute = Cast<UMaterialExpressionReroute>(UV)) { UV = Reroute->Input.Expression; continue; }
			if (auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(UV)) { UV = Usage->Declaration ? Usage->Declaration->Input.Expression : nullptr; continue; }
			break;
		}
		auto* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(UV);
		if (!TexCoord)
		{
			Why = FString::Printf(TEXT("TextureSample 의 UV 입력이 %s 노드다 (TextureCoordinate 만 지원)"), *NodeName(UV));
			return false;
		}
		if (TexCoord->CoordinateIndex != 0)
		{
			Why = FString::Printf(TEXT("TextureCoordinate 가 UV%d 를 쓴다 (UV0 만 지원)"), TexCoord->CoordinateIndex);
			return false;
		}
		OutTiling = FVector2D(TexCoord->UTiling, TexCoord->VTiling);
		return true;
	}

	bool Combine(const FValue& A, const FValue& B, FValue& Out, FString& Why)
	{
		const bool bTexA = A.Kind == FValue::EKind::Texture;
		const bool bTexB = B.Kind == FValue::EKind::Texture;
		if (bTexA && bTexB)
		{
			Why = TEXT("텍스처 두 장을 곱한다 (지원 안 함)");
			return false;
		}
		if (bTexA || bTexB)
		{
			const FValue& Tex = bTexA ? A : B;
			const FValue& Other = bTexA ? B : A;
			Out = Tex;
			Out.Tint = Tex.Tint * Other.AsColor();
			Out.Tint.A = 1.f;
			return true;
		}
		if (A.Kind == FValue::EKind::Scalar && B.Kind == FValue::EKind::Scalar)
		{
			Out = FValue::FromScalar(A.Scalar * B.Scalar);
			return true;
		}
		Out = FValue::FromColor(A.AsColor() * B.AsColor());
		return true;
	}

	bool Resolve(const UMaterialInterface* Source, UMaterialExpression* Expr, int32 OutputIndex, FValue& Out, FString& Why, int32 Depth)
	{
		if (!Expr)
		{
			Why = TEXT("끊긴 입력");
			return false;
		}
		if (Depth > 16)
		{
			Why = TEXT("그래프가 너무 깊다");
			return false;
		}

		// 통과 노드
		if (auto* Reroute = Cast<UMaterialExpressionReroute>(Expr))
		{
			return Resolve(Source, Reroute->Input.Expression, Reroute->Input.OutputIndex, Out, Why, Depth + 1);
		}
		if (auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expr))
		{
			if (!Usage->Declaration)
			{
				Why = TEXT("Named Reroute 선언이 없다");
				return false;
			}
			return Resolve(Source, Usage->Declaration->Input.Expression, Usage->Declaration->Input.OutputIndex, Out, Why, Depth + 1);
		}
		if (auto* Decl = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expr))
		{
			return Resolve(Source, Decl->Input.Expression, Decl->Input.OutputIndex, Out, Why, Depth + 1);
		}

		// 텍스처
		if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expr))
		{
			Out = FValue();
			Out.Kind = FValue::EKind::Texture;
			Out.Channel = OutputIndex;
			if (!ResolveTextureOf(Source, Sample, Out.Texture, Why)) return false;
			if (!ResolveTilingOf(Sample, Out.Tiling, Why)) return false;
			return true;
		}

		// 상수 / 파라미터
		if (auto* C = Cast<UMaterialExpressionConstant>(Expr))
		{
			Out = FValue::FromScalar(C->R);
			return true;
		}
		if (auto* C3 = Cast<UMaterialExpressionConstant3Vector>(Expr))
		{
			FLinearColor V = C3->Constant;
			V.A = 1.f;
			Out = FValue::FromColorPin(V, OutputIndex);
			return true;
		}
		if (auto* C4 = Cast<UMaterialExpressionConstant4Vector>(Expr))
		{
			Out = FValue::FromColorPin(C4->Constant, OutputIndex);
			return true;
		}
		if (auto* SP = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			float V = SP->DefaultValue;
			Source->GetScalarParameterValue(FMaterialParameterInfo(SP->ParameterName), V);
			Out = FValue::FromScalar(V);
			return true;
		}
		if (auto* VP = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			FLinearColor V = VP->DefaultValue;
			Source->GetVectorParameterValue(FMaterialParameterInfo(VP->ParameterName), V);
			Out = FValue::FromColorPin(V, OutputIndex);
			return true;
		}

		// 곱
		if (auto* Mul = Cast<UMaterialExpressionMultiply>(Expr))
		{
			FValue A, B;
			if (Mul->A.Expression)
			{
				if (!Resolve(Source, Mul->A.Expression, Mul->A.OutputIndex, A, Why, Depth + 1)) return false;
			}
			else
			{
				A = FValue::FromScalar(Mul->ConstA);
			}
			if (Mul->B.Expression)
			{
				if (!Resolve(Source, Mul->B.Expression, Mul->B.OutputIndex, B, Why, Depth + 1)) return false;
			}
			else
			{
				B = FValue::FromScalar(Mul->ConstB);
			}
			return Combine(A, B, Out, Why);
		}

		// 상수 접기: 입력이 전부 상수·파라미터면 여기서 계산해 단색으로 만든다. 텍스처가 섞이면 픽셀마다 달라 접을 수 없다.
		// (발광 버튼 머티리얼이 Emissive 에 Desaturation 을, 그래비티 코어가 Power 를 쓴다.)
		if (auto* Desat = Cast<UMaterialExpressionDesaturation>(Expr))
		{
			if (!Desat->Input.Expression)
			{
				Why = TEXT("Desaturation: 입력이 없다");
				return false;
			}
			FValue In;
			if (!Resolve(Source, Desat->Input.Expression, Desat->Input.OutputIndex, In, Why, Depth + 1)) return false;
			if (In.Kind == FValue::EKind::Texture)
			{
				Why = TEXT("Desaturation: 텍스처를 탈색한다 (지원 안 함)");
				return false;
			}
			// 엔진과 같다: Fraction 미연결이면 완전 탈색(Grey), 연결이면 Lerp(Color, Grey, Fraction)
			float Fraction = 1.f;
			if (Desat->Fraction.Expression)
			{
				FValue F;
				if (!Resolve(Source, Desat->Fraction.Expression, Desat->Fraction.OutputIndex, F, Why, Depth + 1)) return false;
				if (F.Kind == FValue::EKind::Texture)
				{
					Why = TEXT("Desaturation: 탈색 비율이 텍스처다 (지원 안 함)");
					return false;
				}
				Fraction = F.Kind == FValue::EKind::Scalar ? F.Scalar : F.Color.R;
			}
			const FLinearColor C = In.AsColor();
			const FLinearColor& L = Desat->LuminanceFactors;
			const float Grey = C.R * L.R + C.G * L.G + C.B * L.B;
			const FLinearColor R = FMath::Lerp(C, FLinearColor(Grey, Grey, Grey, C.A), Fraction);
			Out = FValue::FromColorPin(R, OutputIndex);
			return true;
		}
		if (auto* Pow = Cast<UMaterialExpressionPower>(Expr))
		{
			if (!Pow->Base.Expression)
			{
				Why = TEXT("Power: 밑이 없다");
				return false;
			}
			FValue B;
			if (!Resolve(Source, Pow->Base.Expression, Pow->Base.OutputIndex, B, Why, Depth + 1)) return false;
			if (B.Kind == FValue::EKind::Texture)
			{
				Why = TEXT("Power: 텍스처를 거듭제곱한다 (지원 안 함)");
				return false;
			}
			float Exponent = Pow->ConstExponent;
			if (Pow->Exponent.Expression)
			{
				FValue E;
				if (!Resolve(Source, Pow->Exponent.Expression, Pow->Exponent.OutputIndex, E, Why, Depth + 1)) return false;
				if (E.Kind == FValue::EKind::Texture)
				{
					Why = TEXT("Power: 지수가 텍스처다 (지원 안 함)");
					return false;
				}
				Exponent = E.Kind == FValue::EKind::Scalar ? E.Scalar : E.Color.R;
			}
			// 엔진 셰이더와 같이 밑은 0 이상으로 자른다
			auto PowClamped = [Exponent](float V) { return FMath::Pow(FMath::Max(V, 0.f), Exponent); };
			if (B.Kind == FValue::EKind::Scalar)
			{
				Out = FValue::FromScalar(PowClamped(B.Scalar));
				return true;
			}
			const FLinearColor R(PowClamped(B.Color.R), PowClamped(B.Color.G), PowClamped(B.Color.B), B.Color.A);
			Out = FValue::FromColorPin(R, OutputIndex);
			return true;
		}

		Why = FString::Printf(TEXT("%s 노드는 지원하지 않는다"), *NodeName(Expr));
		return false;
	}

	/** 머티리얼 출력 핀 하나를 따라간다. 미연결이면 Default 를 돌려준다. */
	bool ResolveProperty(const UMaterialInterface* Source, UMaterial* Base, EMaterialProperty Property, const TCHAR* Label, const FValue& Default, FValue& Out, FString& Why)
	{
		FExpressionInput* In = Base->GetExpressionInputForProperty(Property);
		if (!In || !In->Expression)
		{
			Out = Default;
			return true;
		}
		FString Inner;
		if (!Resolve(Source, In->Expression, In->OutputIndex, Out, Inner, 0))
		{
			Why = FString::Printf(TEXT("%s: %s"), Label, *Inner);
			return false;
		}
		return true;
	}

	FLinearColor ChannelMask(int32 Channel)
	{
		switch (Channel)
		{
		case 2: return FLinearColor(0.f, 1.f, 0.f, 0.f);
		case 3: return FLinearColor(0.f, 0.f, 1.f, 0.f);
		case 4: return FLinearColor(0.f, 0.f, 0.f, 1.f);
		default: return FLinearColor(1.f, 0.f, 0.f, 0.f); // RGB/RGBA 를 스칼라에 꽂으면 R 이 쓰인다
		}
	}

	/** Roughness / Metallic 처럼 스칼라 출력 하나를 텍스처+채널 또는 상수로 옮긴다 */
	bool ApplyScalarProperty(const FValue& V, const TCHAR* Label, TSoftObjectPtr<UTexture>& OutTex, FLinearColor& OutChannel, float& OutConst, FString& Why)
	{
		switch (V.Kind)
		{
		case FValue::EKind::Scalar:
			OutConst = V.Scalar;
			return true;
		case FValue::EKind::Color:
			OutConst = V.Color.R;
			return true;
		case FValue::EKind::Texture:
			if (!V.IsWhiteTint())
			{
				Why = FString::Printf(TEXT("%s: 텍스처에 값을 곱한다 (지원 안 함)"), Label);
				return false;
			}
			OutTex = V.Texture;
			OutChannel = ChannelMask(V.Channel);
			return true;
		}
		return false;
	}
}

bool FCSCameraFadeMaterialExtractor::Extract(UMaterialInterface* Source, FCSCameraFadeMaterialData& Out, FString& OutWhy)
{
	UMaterial* Base = Source ? Source->GetMaterial() : nullptr;
	if (!Base)
	{
		OutWhy = TEXT("부모 머티리얼이 없다");
		return false;
	}
	if (Base->MaterialDomain != MD_Surface)
	{
		OutWhy = TEXT("Surface 도메인이 아니다");
		return false;
	}
	if (Base->bUseMaterialAttributes)
	{
		OutWhy = TEXT("MaterialAttributes 를 쓴다 (지원 안 함)");
		return false;
	}
	if (Source->GetBlendMode() != BLEND_Opaque)
	{
		OutWhy = TEXT("Opaque 가 아니다 (Masked/Translucent 는 지원 안 함)");
		return false;
	}

	Out = FCSCameraFadeMaterialData();
	Out.SourceStateId = Base->StateId;	// 호출자(FCSCameraFadeMaterials)가 인스턴스 체인을 섞은 키로 덮어쓴다

	TOptional<FVector2D> Tiling;
	auto CheckTiling = [&](const FValue& V) -> bool
	{
		if (V.Kind != FValue::EKind::Texture) return true;
		if (Tiling.IsSet() && !Tiling->Equals(V.Tiling, 1e-4f))
		{
			OutWhy = TEXT("텍스처마다 UV 타일링이 다르다 (지원 안 함)");
			return false;
		}
		Tiling = V.Tiling;
		return true;
	};

	// BaseColor
	{
		FValue V;
		if (!ResolveProperty(Source, Base, MP_BaseColor, TEXT("BaseColor"), FValue::FromColor(FLinearColor::Black), V, OutWhy)) return false;
		if (!CheckTiling(V)) return false;
		switch (V.Kind)
		{
		case FValue::EKind::Texture:
			if (V.Channel != 0 && V.Channel != 5)
			{
				OutWhy = TEXT("BaseColor: 텍스처의 한 채널만 쓴다 (지원 안 함)");
				return false;
			}
			Out.BaseColorTexture = V.Texture;
			Out.BaseColorTint = V.Tint;
			break;
		default:
			Out.BaseColorTint = V.AsColor();
			Out.BaseColorTint.A = 1.f;
			break;
		}
	}

	// Emissive. 미연결이면 검정(발광 없음). 발광하는 버튼·배터리가 가려지는 순간 빛이 꺼지지 않게 원본 그대로 옮긴다.
	{
		FValue V;
		if (!ResolveProperty(Source, Base, MP_EmissiveColor, TEXT("Emissive"), FValue::FromColor(FLinearColor::Black), V, OutWhy)) return false;
		if (!CheckTiling(V)) return false;
		switch (V.Kind)
		{
		case FValue::EKind::Texture:
			if (V.Channel != 0 && V.Channel != 5)
			{
				OutWhy = TEXT("Emissive: 텍스처의 한 채널만 쓴다 (지원 안 함)");
				return false;
			}
			Out.EmissiveTexture = V.Texture;
			Out.EmissiveColor = V.Tint;
			break;
		default:
			Out.EmissiveColor = V.AsColor();
			Out.EmissiveColor.A = 1.f;
			break;
		}
	}

	// Normal
	{
		FValue V;
		if (!ResolveProperty(Source, Base, MP_Normal, TEXT("Normal"), FValue::FromColor(FLinearColor(0.f, 0.f, 1.f, 1.f)), V, OutWhy)) return false;
		if (!CheckTiling(V)) return false;
		if (V.Kind == FValue::EKind::Texture)
		{
			if ((V.Channel != 0 && V.Channel != 5) || !V.IsWhiteTint())
			{
				OutWhy = TEXT("Normal: 텍스처를 그대로 쓰지 않는다 (지원 안 함)");
				return false;
			}
			Out.NormalTexture = V.Texture;
		}
		else if (!V.AsColor().Equals(FLinearColor(0.f, 0.f, 1.f, 1.f), 1e-3f))
		{
			OutWhy = TEXT("Normal: 평평하지 않은 상수 노멀 (지원 안 함)");
			return false;
		}
	}

	// Roughness / Metallic
	{
		FValue V;
		if (!ResolveProperty(Source, Base, MP_Roughness, TEXT("Roughness"), FValue::FromScalar(0.5f), V, OutWhy)) return false;
		if (!CheckTiling(V)) return false;
		if (!ApplyScalarProperty(V, TEXT("Roughness"), Out.RoughnessTexture, Out.RoughnessChannel, Out.RoughnessConstant, OutWhy)) return false;
	}
	{
		FValue V;
		if (!ResolveProperty(Source, Base, MP_Metallic, TEXT("Metallic"), FValue::FromScalar(0.f), V, OutWhy)) return false;
		if (!CheckTiling(V)) return false;
		if (!ApplyScalarProperty(V, TEXT("Metallic"), Out.MetallicTexture, Out.MetallicChannel, Out.MetallicConstant, OutWhy)) return false;
	}

	Out.UVTiling = Tiling.Get(FVector2D(1.f, 1.f));
	return true;
}

#endif // WITH_EDITOR
