// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Course/CourseDownloads.h"
#include "Course/GolfCourseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

namespace
{
    /**
     * One step of the packaged download check, run in a fresh game process each time (check_course_install.py):
     * absent, install (then play at once, without a restart), play (offline, next start) and remove.
     */
    class FCheckCourseDownload : public IAutomationLatentCommand
    {
    public:
        FCheckCourseDownload(FAutomationTestBase* Owner, FString InAction) : Test(Owner), Action(MoveTemp(InAction)) {}

        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 120)
            {
                Test->AddError(TEXT("Timed out checking the packaged course."));
                return true;
            }
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                UWorld* W = Context.World();
                if (!W || W->WorldType != EWorldType::Game || !W->HasBegunPlay())
                    continue;
                auto* Library = W->GetGameInstance()->GetSubsystem<UCourseDownloads>();
                auto* Courses = W->GetGameInstance()->GetSubsystem<UGolfCourseSubsystem>();
                if (!Courses->IsLoaded())
                    return false;
                return Check(*W, *Library, *Courses);
            }
            return false;
        }

    private:
        static constexpr const TCHAR* Id = TEXT("HunterRanch");
        static constexpr int32 RedTees = 4;

        bool Check(UWorld& W, UCourseDownloads& Library, UGolfCourseSubsystem& Courses)
        {
            const auto* Course = UGolfCourseSubsystem::FindCourse(TEXT("?Course=DA_HunterRanch"));
            if (Action == TEXT("absent"))
            {
                Test->TestFalse(TEXT("Pack is absent"), Library.IsInstalled(Id));
                Test->TestNull(TEXT("Hunter Ranch is absent from the base game"), Course);
                return true;
            }
            if (Action == TEXT("install"))
                return Install(W, Library, Courses, Course);
            if (!Test->TestNotNull(TEXT("Installed course resolves offline"), Course))
                return true;
            if (Action == TEXT("remove"))
            {
                Library.Uninstall(Id);
                Test->TestFalse(TEXT("Uninstalled course leaves the list at once"), Courses.GetCourses().Contains(Course));
                Test->AddInfo(Library.Message());
                return true;
            }
            return Action == TEXT("play") ? Play(W, *Course) : true;
        }

        bool Install(UWorld& W, UCourseDownloads& Library, UGolfCourseSubsystem& Courses, const UGolfCourseDefinition* Course)
        {
            if (Step == 0)
            {
                Library.Refresh();
                ++Step;
                return false;
            }
            if (Library.Busy())
                return false;
            if (Step == 1)
            {
                if (!Test->TestNotNull(TEXT("Public catalog lists Hunter Ranch"), Library.FindEntry(Id)))
                    return true;
                Library.Install(Id);
                ++Step;
                return false;
            }
            if (Step == 2)
            {
                Test->AddInfo(Library.Message());
                if (!Test->TestTrue(TEXT("Pack installed"), Library.IsInstalled(Id)) ||
                    !Test->TestNotNull(TEXT("Installed course plays without a restart"), Course))
                    return true;
                Test->TestTrue(TEXT("Installed course joins the list"), Courses.GetCourses().Contains(Course));
                ++Step;
            }
            return Play(W, *Course);
        }

        bool Play(UWorld& W, const UGolfCourseDefinition& Course)
        {
            if (!bStarted)
            {
                auto* Controller = Cast<AUOpenGolfPlayerController>(W.GetFirstPlayerController());
                if (!Controller)
                    return false;
                OpenGolfPlay::FOpenGolfRoundSetup Setup;
                Setup.TeeIndex = RedTees;
                Controller->StartCourse(Course, Setup);
                bStarted = true;
                return false;
            }
            auto* Mode = W.GetAuthGameMode<AUOpenGolfGameMode>();
            if (!Mode || Mode->GetCourse() != &Course)
                return false;
            Test->TestEqual(TEXT("Downloaded round has 18 holes"), Course.Layout.Num(), 18);
            Test->TestEqual(TEXT("Red tees loaded"), Mode->GetHoleLengthM(), Course.Layout[0].Tees[RedTees].PublishedYards * 0.9144f);
            Test->TestTrue(TEXT("Ball starts on the tee"), Mode->IsBallOnTee());
            return true;
        }

        FAutomationTestBase* Test;
        FString Action;
        int32 Step = 0;
        bool bStarted = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourseDownloadTest,"UOpenGolf.Course.PackagedDownload",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCourseDownloadTest::RunTest(const FString&)
{
    FString Action;
    if (!FParse::Value(FCommandLine::Get(),TEXT("CourseTestAction="),Action))
    {
        AddInfo(TEXT("Supply -CourseTestAction=absent, install, play or remove for the packaged course test."));
        return true;
    }
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckCourseDownload>(this,Action));
    return true;
}
#endif
