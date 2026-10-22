// SPDX-License-Identifier: MIT
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "OpenGolfWindIndicator.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
    void CreateWindIndicatorAsset()
    {
        const FString PackageName(TEXT("/Game/UOpenGolf/UI/WBP_WindIndicator"));
        if (FPackageName::DoesPackageExist(PackageName))
        {
            UE_LOG(LogTemp, Display, TEXT("Wind indicator already exists; keeping its edits."));
            return;
        }
        UPackage* Package = CreatePackage(*PackageName);
        UWidgetBlueprint* Blueprint = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, TEXT("WBP_WindIndicator"), BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
        UWidgetTree* Tree = Blueprint->WidgetTree;
        UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
        Tree->RootWidget = Canvas;
        Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOpenGolfWindIndicator* Indicator = Tree->ConstructWidget<UOpenGolfWindIndicator>(UOpenGolfWindIndicator::StaticClass(), TEXT("WindIndicator"));
        Indicator->SetVisibility(ESlateVisibility::HitTestInvisible);
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Indicator);
        Slot->SetAnchors(FAnchors(0.5f, 0));
        Slot->SetAlignment(FVector2D(0.5f, 0));
        Slot->SetPosition(FVector2D(0, 20));
        Slot->SetAutoSize(true);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status == BS_Error)
        {
            UE_LOG(LogTemp, Error, TEXT("Wind indicator Blueprint did not compile."));
            return;
        }
        FAssetRegistryModule::AssetCreated(Blueprint);
        Package->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
        {
            UE_LOG(LogTemp, Error, TEXT("Could not save wind indicator Blueprint."));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Created %s"), *PackageName);
        }
    }

    FAutoConsoleCommand CreateWindWidgetCommand(TEXT("uopengolf.CreateWindIndicatorAsset"),
        TEXT("Create the wind Widget Blueprint if it does not exist; preserves existing assets."),
        FConsoleCommandDelegate::CreateStatic(&CreateWindIndicatorAsset));
} // namespace
#endif
