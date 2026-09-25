// Fill out your copyright notice in the Description page of Project Settings.

// 프로젝트 루트 .mcp.json 의 unreal-mcp 엔트리를 에디터 시작 시 보장한다.
//
// 왜 커밋하지 않고 생성하는가:
//   .mcp.json 은 MCP 클라이언트가 공유하는 파일이지만, NarshaMCP 등 일부 도구가
//   자기 엔트리를 여기에 직접 써넣는다. 그 엔트리에는 플러그인 경로, 프로젝트 경로,
//   엔진 경로, 홈 디렉터리가 절대경로로 박히므로 사람마다 값이 다르다.
//   커밋하면 각자 빌드·에디터 실행 때마다 서로 되돌리는 diff 가 난다.
//
//   그래서 .mcp.json 은 .gitignore 로 내리고, 팀이 공유해야 하는 unreal-mcp
//   엔트리만 이 코드가 채운다. clone 후 에디터를 한 번 켜면 준비가 끝나므로
//   팀원이 따로 해야 할 설정이 없다.
//
// 동작 규칙:
//   파일 없음            → unreal-mcp 하나만 담아 새로 만든다
//   있고 엔트리 없음     → 기존 내용을 보존한 채 unreal-mcp 만 추가한다
//   있고 엔트리 있음     → 아무것도 하지 않는다
//   파싱 실패            → 손대지 않는다. 덮어쓰면 남의 설정이 날아간다

#include "Editor/CSMcpConfigBootstrap.h"

#if WITH_EDITOR

#include "ChronoSpace.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* GMcpConfigFileName = TEXT(".mcp.json");
	const TCHAR* GServersFieldName  = TEXT("mcpServers");
	const TCHAR* GUnrealMcpKey      = TEXT("unreal-mcp");

	/** 엔진 내장 ModelContextProtocol 플러그인이 여는 주소. 프로젝트와 무관하게 고정이다. */
	const TCHAR* GUnrealMcpUrl      = TEXT("http://127.0.0.1:8000/mcp");

	TSharedRef<FJsonObject> MakeUnrealMcpEntry()
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("type"), TEXT("http"));
		Entry->SetStringField(TEXT("url"), GUnrealMcpUrl);
		return Entry;
	}

	bool WriteConfig(const FString& FilePath, const TSharedRef<FJsonObject>& Root)
	{
		FString Output;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);

		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			return false;
		}

		// 다른 MCP 도구가 같은 파일을 읽는다. BOM 없는 UTF-8 로 맞춘다.
		return FFileHelper::SaveStringToFile(Output, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

void FCSMcpConfigBootstrap::EnsureProjectMcpConfig()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), GMcpConfigFileName);

	FString Existing;
	if (!FFileHelper::LoadFileToString(Existing, *FilePath))
	{
		TSharedRef<FJsonObject> Servers = MakeShared<FJsonObject>();
		Servers->SetObjectField(GUnrealMcpKey, MakeUnrealMcpEntry());

		TSharedRef<FJsonObject> NewRoot = MakeShared<FJsonObject>();
		NewRoot->SetObjectField(GServersFieldName, Servers);

		if (WriteConfig(FilePath, NewRoot))
		{
			UE_LOG(LogCS, Log, TEXT("MCP 설정을 새로 만들었다: %s"), *FilePath);
		}
		else
		{
			UE_LOG(LogCS, Warning, TEXT("MCP 설정을 만들지 못했다: %s"), *FilePath);
		}
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Existing);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogCS, Warning, TEXT("MCP 설정을 파싱하지 못해 건너뛴다: %s"), *FilePath);
		return;
	}

	const TSharedPtr<FJsonObject>* ServersField = nullptr;
	if (Root->TryGetObjectField(GServersFieldName, ServersField) && ServersField != nullptr && ServersField->IsValid())
	{
		if ((*ServersField)->HasField(GUnrealMcpKey))
		{
			return;
		}

		// 다른 도구의 엔트리는 그대로 두고 우리 것만 끼워 넣는다.
		(*ServersField)->SetObjectField(GUnrealMcpKey, MakeUnrealMcpEntry());
	}
	else
	{
		TSharedRef<FJsonObject> Servers = MakeShared<FJsonObject>();
		Servers->SetObjectField(GUnrealMcpKey, MakeUnrealMcpEntry());
		Root->SetObjectField(GServersFieldName, Servers);
	}

	if (WriteConfig(FilePath, Root.ToSharedRef()))
	{
		UE_LOG(LogCS, Log, TEXT("MCP 설정에 unreal-mcp 엔트리를 추가했다: %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("MCP 설정을 저장하지 못했다: %s"), *FilePath);
	}
}

#endif // WITH_EDITOR
