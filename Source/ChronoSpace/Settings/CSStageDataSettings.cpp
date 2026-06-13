// Fill out your copyright notice in the Description page of Project Settings.


#include "Settings/CSStageDataSettings.h"

const UCSStageDataSettings* UCSStageDataSettings::Get()
{
    return GetDefault<UCSStageDataSettings>();
}

const FCSChapterDef* UCSStageDataSettings::FindChapter(int32 Chapter) const
{
    return Chapters.FindByPredicate([Chapter](const FCSChapterDef& C)
    {
        return C.Chapter == Chapter;
    });
}

const FCSStageDef* UCSStageDataSettings::FindStageDef(int32 Chapter, int32 Stage) const
{
    const FCSChapterDef* Ch = FindChapter(Chapter);
    if (!Ch) return nullptr;
    return Ch->Stages.FindByPredicate([Stage](const FCSStageDef& S)
    {
        return S.Stage == Stage;
    });
}

bool UCSStageDataSettings::IsValidStage(int32 Chapter, int32 Stage) const
{
    return FindStageDef(Chapter, Stage) != nullptr;
}

int32 UCSStageDataSettings::GetLastStageNumber(int32 Chapter) const
{
    const FCSChapterDef* Ch = FindChapter(Chapter);
    if (!Ch || Ch->Stages.Num() == 0) return 0;

    int32 Last = 0;
    for (const FCSStageDef& S : Ch->Stages)
    {
        Last = FMath::Max(Last, S.Stage);
    }
    return Last;
}

bool UCSStageDataSettings::IsLastStageOfChapter(int32 Chapter, int32 Stage) const
{
    const int32 Last = GetLastStageNumber(Chapter);
    return Last > 0 && Stage == Last;
}

bool UCSStageDataSettings::GetFirstStage(int32& OutChapter, int32& OutStage) const
{
    const FCSChapterDef* Best = nullptr;
    for (const FCSChapterDef& C : Chapters)
    {
        if (C.Stages.Num() == 0) continue;
        if (!Best || C.Chapter < Best->Chapter)
        {
            Best = &C;
        }
    }
    if (!Best) return false;

    int32 SmallestStage = MAX_int32;
    for (const FCSStageDef& S : Best->Stages)
    {
        SmallestStage = FMath::Min(SmallestStage, S.Stage);
    }
    OutChapter = Best->Chapter;
    OutStage = SmallestStage;
    return true;
}

bool UCSStageDataSettings::GetNextStage(int32 Chapter, int32 Stage, int32& OutChapter, int32& OutStage) const
{
    const FCSChapterDef* Ch = FindChapter(Chapter);
    if (!Ch) return false;

    // Smallest stage greater than the current one, within this chapter.
    int32 Best = MAX_int32;
    bool bFound = false;
    for (const FCSStageDef& S : Ch->Stages)
    {
        if (S.Stage > Stage && S.Stage < Best)
        {
            Best = S.Stage;
            bFound = true;
        }
    }
    if (bFound)
    {
        OutChapter = Chapter;
        OutStage = Best;
        return true;
    }

    // Otherwise the first stage of the next chapter.
    const FCSChapterDef* Next = FindChapter(Chapter + 1);
    if (!Next || Next->Stages.Num() == 0) return false;

    int32 Smallest = MAX_int32;
    for (const FCSStageDef& S : Next->Stages)
    {
        Smallest = FMath::Min(Smallest, S.Stage);
    }
    OutChapter = Chapter + 1;
    OutStage = Smallest;
    return true;
}

bool UCSStageDataSettings::GetPrevStage(int32 Chapter, int32 Stage, int32& OutChapter, int32& OutStage) const
{
    const FCSChapterDef* Ch = FindChapter(Chapter);
    if (!Ch) return false;

    // Largest stage smaller than the current one, within this chapter.
    int32 Best = 0;
    bool bFound = false;
    for (const FCSStageDef& S : Ch->Stages)
    {
        if (S.Stage < Stage && S.Stage > Best)
        {
            Best = S.Stage;
            bFound = true;
        }
    }
    if (bFound)
    {
        OutChapter = Chapter;
        OutStage = Best;
        return true;
    }

    // Otherwise the last stage of the previous chapter.
    const int32 PrevLast = GetLastStageNumber(Chapter - 1);
    if (PrevLast <= 0) return false;

    OutChapter = Chapter - 1;
    OutStage = PrevLast;
    return true;
}

FString UCSStageDataSettings::GetStageTravelURL(int32 Chapter, int32 Stage) const
{
    const FCSStageDef* Def = FindStageDef(Chapter, Stage);
    if (!Def) return FString();

    const FSoftObjectPath Path = Def->Level.ToSoftObjectPath();
    if (!Path.IsValid()) return FString();

    return Path.GetLongPackageName();
}

FText UCSStageDataSettings::GetStageDisplayName(int32 Chapter, int32 Stage) const
{
    const FCSStageDef* Def = FindStageDef(Chapter, Stage);
    if (Def && !Def->DisplayName.IsEmpty())
    {
        return Def->DisplayName;
    }
    return FText::FromString(FString::Printf(TEXT("C%d-S%d"), Chapter, Stage));
}
