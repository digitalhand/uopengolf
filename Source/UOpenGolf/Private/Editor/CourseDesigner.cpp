// SPDX-License-Identifier: MIT
#if WITH_EDITOR
#include "Editor.h"
#include "Async/Async.h"
#include "DesktopPlatformModule.h"
#include "IPythonScriptPlugin.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/Selection.h"
#include "FileHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "GolfCourseDefinition.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfTerrainBuilder.h"
#include "PlayInEditorDataTypes.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "UOpenGolfRange.h"
#include "EngineUtils.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace CourseDesigner
{
    UWorld* World()
    {
        return GEditor && !GEditor->PlayWorld ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    class SLayout : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SLayout) {} SLATE_END_ARGS()
        void Construct(const FArguments&) {}
        TWeakObjectPtr<AOpenGolfTerrainArea> Area;
        TWeakObjectPtr<UGolfCourseDefinition> Course;
        int32 Hole = 0;
        int32 Tee = 0;
        FString Status;
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(700, 470); }
        FVector2D Pixel(const FGeometry& G, FVector2D M) const
        {
            const auto& D = *Area->Definition;
            const FVector2D Span = D.MaximumM - D.MinimumM;
            const FVector2D Size = G.GetLocalSize();
            const double Scale = FMath::Min(Size.X / Span.X, Size.Y / Span.Y);
            const FVector2D P = M - (D.MinimumM + D.MaximumM) * 0.5;
            return Size * 0.5 + FVector2D(P.X, -P.Y) * Scale;
        }
        bool Ready() const
        {
            return World() && Course.IsValid() && Area.IsValid() && Area->GetWorld() == World() && Area->Definition && Course->Layout.IsValidIndex(Hole);
        }
        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
            int32 Layer, const FWidgetStyle&, bool) const override
        {
            if (!Ready()) return Layer;
            auto Line = [&](const TArray<FVector2f>& P, FLinearColor Color, float Width)
            {
                FSlateDrawElement::MakeLines(Out, Layer + 1, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Color, true, Width);
            };
            for (const auto& SurfaceRegion : Area->Definition->Regions)
            {
                if (SurfaceRegion.OutlineM.IsEmpty() || !SurfaceRegion.Surface) continue;
                TArray<FVector2f> Path;
                for (const auto& P : SurfaceRegion.OutlineM) Path.Emplace(Pixel(G, P));
                Path.Add(Path[0]);
                FLinearColor Ink(0.28, 0.57, 0.34);
                if (SurfaceRegion.Surface->Surface == EOpenFairwaySurface::Sand) Ink = FLinearColor(0.9, 0.76, 0.50);
                if (SurfaceRegion.Surface->Surface == EOpenFairwaySurface::Green) Ink = FLinearColor(0.66, 0.9, 0.37);
                Line(Path, Ink, 1);
            }
            const auto& H = Course->Layout[Hole];
            if (!H.Tees.IsValidIndex(Tee)) return Layer + 1;
            TArray<FVector2f> Route;
            auto Add = [&](FVector P) { Route.Emplace(Pixel(G, FVector2D(P) / 100)); };
            Add(H.Tees[Tee].PositionCm);
            for (FVector P : H.RoutingCm) Add(P);
            Add(H.CupCm);
            Line(Route, FLinearColor::White, 2);
            for (const auto& P : Route)
                Line({P - FVector2f(3,3), P + FVector2f(3,3), P + FVector2f(3,-3), P - FVector2f(3,3)}, FLinearColor::Yellow, 2);
            return Layer + 1;
        }
        virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
        {
            if (!Ready() || E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
            auto& H = Course->Layout[Hole];
            if (!H.Tees.IsValidIndex(Tee)) return FReply::Unhandled();
            const FVector2D Cursor = G.AbsoluteToLocal(E.GetScreenSpacePosition());
            double Best = 10;
            Vertex = INDEX_NONE;
            auto Consider = [&](FVector2D M, int32 CandidateRegion, int32 Candidate)
            {
                const double D = FVector2D::Distance(Cursor, Pixel(G, M));
                if (D <= Best) { Best = D; Region = CandidateRegion; Vertex = Candidate; OldPoint = M; }
            };
            for (int32 R = 0; R < Area->Definition->Regions.Num(); ++R)
            {
                const auto& Ring = Area->Definition->Regions[R].OutlineM;
                for (int32 V = 0; V < Ring.Num(); ++V) Consider(Ring[V], R, V);
            }
            Consider(FVector2D(H.Tees[Tee].PositionCm)/100, INDEX_NONE, 0);
            for (int32 I = 0; I < H.RoutingCm.Num(); ++I) Consider(FVector2D(H.RoutingCm[I])/100, INDEX_NONE, I+1);
            Consider(FVector2D(H.CupCm)/100, INDEX_NONE, H.RoutingCm.Num()+1);
            if (Vertex == INDEX_NONE) return FReply::Unhandled();
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
        virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override
        {
            if (!HasMouseCapture()) return FReply::Unhandled();
            if (Ready() && Vertex != INDEX_NONE)
            {
                const FVector2D Cursor = G.AbsoluteToLocal(E.GetScreenSpacePosition());
                const FVector2D Origin = Pixel(G, FVector2D::ZeroVector);
                const double Scale = Pixel(G, FVector2D(1,0)).X - Origin.X;
                const FVector2D M((Cursor.X-Origin.X)/Scale, (Origin.Y-Cursor.Y)/Scale);
                if (!M.Equals(OldPoint, 0.05)) Apply(M);
            }
            Vertex = INDEX_NONE;
            return FReply::Handled().ReleaseMouseCapture();
        }
    private:
        int32 Region = INDEX_NONE, Vertex = INDEX_NONE;
        FVector2D OldPoint;
        void Apply(FVector2D P)
        {
            FScopedTransaction Transaction(NSLOCTEXT("CourseDesigner","Move","Move course feature"));
            if (Region == INDEX_NONE)
            {
                Course->Modify();
                auto& H = Course->Layout[Hole];
                FVector& Target = Vertex == 0 ? H.Tees[Tee].PositionCm :
                    Vertex > H.RoutingCm.Num() ? H.CupCm : H.RoutingCm[Vertex-1];
                Target.X = P.X * 100;
                Target.Y = P.Y * 100;
                Course->MarkPackageDirty();
                Status = TEXT("Hole marker moved. Save the course to keep it.");
                return;
            }
            auto* D = Area->Definition.Get();
            if (!FBox2D(D->MinimumM, D->MaximumM).IsInsideOrOn(P)) return;
            D->Modify();
            auto& Ring = D->Regions[Region].OutlineM;
            const FBox2D Before(Ring);
            Ring[Vertex] = P;
            FBox2D Bounds = Before + FBox2D(Ring);
            const FString Folder = FPackageName::GetLongPackagePath(Course->GetOutermost()->GetName()) / TEXT("Tiles");
            FString Error;
            for (int32 Y = FMath::FloorToInt(Bounds.Min.Y/64); Y <= FMath::FloorToInt(Bounds.Max.Y/64); ++Y)
                for (int32 X = FMath::FloorToInt(Bounds.Min.X/64); X <= FMath::FloorToInt(Bounds.Max.X/64); ++X)
                    if (!UOpenGolfTerrainBuilder::BuildCourseTile(Area.Get(), {X,Y}, Folder, Error))
                    {
                        Status = Error + TEXT(" Undo this edit before saving.");
                        return;
                    }
            D->MarkPackageDirty();
            Status = TEXT("Surface moved and ground rebuilt. Use Save to keep the edit.");
        }
    };

    class SDesigner : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SDesigner) {} SLATE_END_ARGS()
        void Construct(const FArguments&)
        {
            SAssignNew(Layout, SLayout);
            ChildSlot[SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Prepare site data…"), [this]{ Prepare(); })]
                    + SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Destination, SEditableTextBox)
                        .HintText(FText::FromString(TEXT("Empty content folder for the new draft, such as /<Course>/Draft")))]
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Build new draft"), [this]{ Build(); })]]
                + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Path, SEditableTextBox)
                        .HintText(FText::FromString(TEXT("Course definition, such as /<Course>/Draft/DA_<Course>")))]
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Open course"), [this]{ Open(); })]
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Play hole"), [this]{ Play(); })]
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Save"), [this]{ Save(); })]
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Save corrections"), [this]{ SaveCorrections(); })]]
                + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("Hole ")))]
                    + SHorizontalBox::Slot().AutoWidth()[SNew(SSpinBox<int32>).MinValue(1).MaxValue_Lambda([this]{return Layout->Course.IsValid() ? Layout->Course->Layout.Num() : 1;})
                        .Value_Lambda([this]{return Layout->Hole+1;}).OnValueChanged_Lambda([this](int32 N){Layout->Hole=N-1;})]
                    + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Change tee"), [this]{
                        if (Layout->Ready()) Layout->Tee=(Layout->Tee+1)%FMath::Max(1,Layout->Course->Layout[Layout->Hole].Tees.Num()); })]
                    + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([this]{
                        if (!Layout->Ready()) return FText();
                        const auto& Tees=Layout->Course->Layout[Layout->Hole].Tees;
                        return Tees.IsValidIndex(Layout->Tee) ? FText::FromName(Tees[Layout->Tee].SetName) : FText(); })]]
                + SVerticalBox::Slot().FillHeight(1)[Layout.ToSharedRef()]
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Layout->Status);})]];
            Layout->Status = TEXT("Open a course definition to edit its layout.");
        }
    private:
        TSharedPtr<SLayout> Layout;
        TSharedPtr<SEditableTextBox> Path, Destination;
        FString Source;
        bool bPreparing = false;
        FString Scripts() const
        {
            return FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(TEXT("OpenGolfCourse"))->GetBaseDir() / TEXT("Scripts"));
        }
        void Prepare()
        {
            if (bPreparing || !World()) return;
            TArray<FString> Files;
            if (!FDesktopPlatformModule::Get()->OpenFileDialog(nullptr, TEXT("Select a site recipe"), FPaths::ConvertRelativePathToFull(
                FPaths::ProjectDir() / TEXT("Assets/Data/CourseDesign")), FString(), TEXT("Course site (*.json)|*.json"), 0, Files)) return;
            const FString Site = Files[0];
            const FString Name = FPaths::GetBaseFilename(Site);
            Source = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CourseDesign") / Name);
            // A course's content plugin mounts at /<recipe name>, as Plugins/HunterRanch does for HunterRanch.json.
            if (Destination->GetText().IsEmpty())
                Destination->SetText(FText::FromString(TEXT("/") + Name + TEXT("/Draft")));
            if (Path->GetText().IsEmpty())
                Path->SetText(FText::FromString(FString::Printf(TEXT("/%s/Draft/DA_%s"), *Name, *Name)));
            const FString Script = Scripts() / TEXT("prepare_course_site.py");
            const FString Args = FString::Printf(TEXT("\"%s\" \"%s\" \"%s\""), *Script, *Site, *Source);
            const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENGOLF_GIS_PYTHON"));
            const FString Python = Override.IsEmpty() ? FString(PLATFORM_WINDOWS ? TEXT("python") : TEXT("python3")) : Override;
            bPreparing = true;
            Layout->Status = TEXT("Preparing course sources…");
            TWeakPtr<SDesigner> Weak=SharedThis(this);
            Async(EAsyncExecution::ThreadPool,[Weak,Python,Args]{
                int32 Code=-1;
                FString Out, Error;
                FPlatformProcess::ExecProcess(*Python,*Args,&Code,&Out,&Error);
                AsyncTask(ENamedThreads::GameThread,[Weak,Code,Out,Error]{
                    if (auto Self=Weak.Pin())
                    {
                        Self->bPreparing=false;
                        Self->Layout->Status= Code==0 ? Out.TrimStartAndEnd() : TEXT("Preparation failed. See Output Log.");
                        if (Code!=0) UE_LOG(LogTemp,Error,TEXT("Course preparation: %s"),*Error);
                    }
                });
            });
        }
        /** The prepared source folder: the one just prepared, or Saved/CourseDesign/<mount> for the open course. */
        FString SourceFolder(const FString& ContentPath) const
        {
            if (!Source.IsEmpty()) return Source;
            FString Mount = ContentPath;
            Mount.RemoveFromStart(TEXT("/"));
            Mount.Split(TEXT("/"), &Mount, nullptr);
            return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CourseDesign") / Mount);
        }
        void Build()
        {
            if (!World() || bPreparing) return;
            const FString Target=Destination->GetText().ToString();
            if (Target.IsEmpty()) { Layout->Status=TEXT("Prepare site data or enter an empty content folder first."); return; }
            Source=SourceFolder(Target);
            FPlatformMisc::SetEnvironmentVar(TEXT("OPENGOLF_DESIGN_SOURCE"),*Source);
            FPlatformMisc::SetEnvironmentVar(TEXT("OPENGOLF_DESIGN_DEST"),*Target);
            auto* Python=IPythonScriptPlugin::Get();
            const bool Ok=Python && Python->ExecPythonCommand(*(Scripts()/TEXT("build_course_draft.py")));
            Layout->Status=Ok ? TEXT("Draft built. Open its course definition to edit.") : TEXT("Draft build failed. See Output Log.");
        }
        TSharedRef<SWidget> Button(const TCHAR* Text, TFunction<void()> Action)
        {
            return SNew(SButton).Text(FText::FromString(Text)).OnClicked_Lambda([Action=MoveTemp(Action)]{Action(); return FReply::Handled();});
        }
        void Open()
        {
            if (!World()) return;
            FString Name=Path->GetText().ToString();
            if (!Name.Contains(TEXT("."))) Name += TEXT(".") + FPackageName::GetShortName(Name);
            auto* Course=LoadObject<UGolfCourseDefinition>(nullptr,*Name);
            if (!Course || Course->Layout.IsEmpty()) { Layout->Status=TEXT("Choose a course definition containing hole records."); return; }
            if (World()->GetOutermost()->GetName()!=Course->Map.ToSoftObjectPath().GetLongPackageName())
            {
                if (!FEditorFileUtils::SaveDirtyPackages(true,true,true) ||
                    !FEditorFileUtils::LoadMap(Course->Map.ToSoftObjectPath().GetLongPackageName(),false,true)) return;
            }
            Layout->Course=Course;
            Layout->Area.Reset();
            TActorIterator<AOpenGolfTerrainArea> It(World());
            if (It) Layout->Area=*It;
            Layout->Hole=Layout->Tee=0;
            Layout->Status=TEXT("Drag a yellow marker or an outline vertex. Ctrl+Z undoes an edit.");
        }
        /** Saves the map, course and terrain, then removes ground squares and lakes that the saved map no longer uses. */
        void Save()
        {
            if (!Layout->Ready() || !FEditorFileUtils::SaveDirtyPackages(false, true, true)) return;
            const FString Root = FPackageName::GetLongPackagePath(Layout->Course->GetOutermost()->GetName());
            auto* Python=IPythonScriptPlugin::Get();
            const bool Ok=Python && Python->ExecPythonCommand(*FString::Printf(TEXT("\"%s\" --course --root %s/Tiles --root %s/Water --apply"),
                *(Scripts()/TEXT("generated_folders.py")), *Root, *Root));
            Layout->Status=Ok ? TEXT("Saved. Replaced ground squares were removed; see the Output Log.") : TEXT("Saved, but old ground squares could not be checked. See Output Log.");
        }
        void SaveCorrections()
        {
            if (!Layout->Ready()) return;
            const FString CoursePath=Layout->Course->GetPathName();
            auto* Python=IPythonScriptPlugin::Get();
            const bool Ok=Python && Python->ExecPythonCommand(*FString::Printf(TEXT("\"%s\" --source \"%s\" --course %s"),
                *(Scripts()/TEXT("SaveCourseCorrections.py")), *SourceFolder(CoursePath), *CoursePath));
            Layout->Status=Ok ? TEXT("Corrections saved. The next draft build keeps them.") : TEXT("Could not save corrections. See Output Log.");
        }
        void Play()
        {
            if (!Layout->Ready()) return;
            FRequestPlaySessionParams Params;
            Params.GlobalMapOverride = Layout->Course->Map.ToSoftObjectPath().GetLongPackageName() +
                FString::Printf(TEXT("?Course=%s?TeeIndex=%d?PreviewHole=%d"), *Layout->Course->GetName(), Layout->Tee, Layout->Hole+1);
            GEditor->RequestPlaySession(Params);
        }
    };

    void Register()
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("OpenGolfCourseDesigner"), FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            { return SNew(SDockTab)[SNew(SDesigner)]; })).SetDisplayName(FText::FromString(TEXT("Course Designer")));
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]{
            auto& Section=UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))->FindOrAddSection(TEXT("Golf"));
            Section.AddMenuEntry(TEXT("OpenGolfDesigner"),FText::FromString(TEXT("Course Designer")),FText(),FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([]{FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("OpenGolfCourseDesigner")));})));
        }));
    }
}
#endif
