// SPDX-License-Identifier: MIT
#include "OpenGolfTerrainBuilder.h"
#include "OpenGolfTerrain.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Data/PCGPointData.h"
#include "Elements/IO/PCGLoadAssetElement.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "OpenFairwayBall.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Misc/PackageName.h"
#include "PCGComponent.h"
#include "PCGDataAsset.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/SavePackage.h"

namespace
{
    // The engine cylinder is 100 cm across and tall, centred on its pivot, with simple collision.
    const TCHAR* TrunkColliderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    // Colliders reach this far below the ground so a sloped base leaves no gap under a trunk.
    constexpr double ColliderSinkCm = 50;
    // The course grass layer (tiled fill and boundary strips) is rebuilt on its own, without touching other vegetation.
    const FName CourseGrassTag(TEXT("OpenGolf.Terrain.CourseGrass"));
    const FName PendingCourseGrassTag(TEXT("OpenGolf.Terrain.PendingCourseGrass"));

    bool SaveVegetationAsset(UObject* Asset)
    {
        FAssetRegistryModule::AssetCreated(Asset);
        Asset->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Asset->GetOutermost(), Asset,
            *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
    }

    bool IsVegetationActor(const AActor* Actor)
    {
        return Actor->FindComponentByClass<UPCGComponent>() || Actor->ActorHasTag(AOpenGolfTerrainArea::VegetationTag) ||
               Actor->ActorHasTag(AOpenGolfTerrainArea::TreesTag) || Actor->ActorHasTag(AOpenGolfTerrainArea::TreeCollidersTag) ||
               Actor->ActorHasTag(AOpenGolfTerrainArea::TreeCanopyTag);
    }

    struct FScatterJob
    {
        FString Name;
        const UOpenGolfVegetationPreset* Preset = nullptr;
        const TArray<FVector2D>* OutlineM = nullptr;
        TArray<FOpenGolfScatterPoint> Points;
        bool bCourseGrass = false;
        /** The meshes these points use: one tile size of a tiled preset, otherwise the preset's meshes. */
        TArray<UStaticMesh*> Meshes;
    };

    bool HasValidMeshes(const UOpenGolfVegetationPreset& Preset)
    {
        if (Preset.PlacementPattern == EOpenGolfPlacementPattern::Tiled)
        {
            for (const auto& Tile : Preset.Tiles)
                if (Tile.Meshes.IsEmpty() || Tile.Meshes.Contains(nullptr))
                    return false;
            return !Preset.Tiles.IsEmpty();
        }
        return !Preset.Meshes.IsEmpty() && !Preset.Meshes.Contains(nullptr);
    }

    /** Gives each job its meshes. A tiled job becomes one job per tile size, because each size has its own meshes. */
    TArray<FScatterJob> SplitByMeshes(TArray<FScatterJob>&& Jobs)
    {
        TArray<FScatterJob> Result;
        for (auto& Job : Jobs)
        {
            if (Job.Preset->PlacementPattern != EOpenGolfPlacementPattern::Tiled)
            {
                Job.Meshes = TArray<UStaticMesh*>(Job.Preset->Meshes);
                Result.Add(MoveTemp(Job));
                continue;
            }
            for (int32 Tile = 0; Tile < Job.Preset->Tiles.Num(); ++Tile)
            {
                FScatterJob& Part = Result.AddDefaulted_GetRef();
                Part.Name = FString::Printf(TEXT("%s %gm"), *Job.Name, Job.Preset->Tiles[Tile].SizeM);
                Part.Preset = Job.Preset;
                Part.OutlineM = Job.OutlineM;
                Part.bCourseGrass = Job.bCourseGrass;
                Part.Meshes = TArray<UStaticMesh*>(Job.Preset->Tiles[Tile].Meshes);
                Part.Points = Job.Points.FilterByPredicate([Tile](const FOpenGolfScatterPoint& Point) { return Point.Tile == Tile; });
            }
        }
        return Result;
    }

    // Each tree is a Chaos body through its trunk collider; keep areas within a sensible physics budget.
    constexpr int32 MaxTreesPerArea = 5000;

    AActor* MakeTreeColliders(AOpenGolfTerrainArea* Area, const UOpenGolfVegetationPreset& Preset, const TArray<FOpenGolfScatterPoint>& Points,
        const FString& Name, FString& Error)
    {
        auto* Mesh = Preset.TreeCollisionMesh ? Preset.TreeCollisionMesh.Get() : LoadObject<UStaticMesh>(nullptr, TrunkColliderMeshPath);
        if (!Mesh)
        {
            Error = TEXT("Could not load the trunk collider mesh.");
            return nullptr;
        }
        auto* Actor = Area->GetWorld()->SpawnActor<AActor>();
        Actor->SetOwner(Area);
        Actor->Tags.Append({AOpenGolfTerrainArea::GeneratedTag, AOpenGolfTerrainArea::TreeCollidersTag, AOpenFairwayBall::GetObstacleTag()});
        Actor->SetActorLabel(FString::Printf(TEXT("Tree colliders %s"), *Name));
        Actor->SetFolderPath(TEXT("Terrain/Trees"));
        auto* Colliders = NewObject<UInstancedStaticMeshComponent>(Actor, TEXT("TrunkColliders"), RF_Transactional);
        Colliders->SetMobility(EComponentMobility::Static);
        Colliders->SetStaticMesh(Mesh);
        // Blocks Visibility, the ball's contact channel. Hidden components keep their collision.
        Colliders->SetCollisionProfileName(TEXT("BlockAll"));
        Colliders->SetVisibility(false);
        Colliders->SetHiddenInGame(true);
        Colliders->SetCastShadow(false);
        Colliders->SetCanEverAffectNavigation(false);
        Colliders->ComponentTags.Add(AOpenFairwayBall::GetObstacleTag());
        Actor->SetRootComponent(Colliders);
        Actor->AddInstanceComponent(Colliders);
        Colliders->RegisterComponent();
        TArray<FTransform> Transforms;
        Transforms.Reserve(Points.Num());
        for (const auto& Point : Points)
        {
            if (Preset.TreeCollisionMesh)
            {
                Transforms.Add(Point.Transform);
                continue;
            }
            const double Scale = Point.Transform.GetScale3D().Z;
            const double HeightCm = Preset.TrunkColliderHeightM * Scale * 100 + ColliderSinkCm;
            const double Diameter = Preset.TrunkColliderRadiusM * Scale * 2;
            Transforms.Emplace(Point.Transform.GetRotation(), Point.Transform.GetLocation() + FVector(0, 0, HeightCm * 0.5 - ColliderSinkCm),
                FVector(Diameter, Diameter, HeightCm / 100));
        }
        Colliders->AddInstances(Transforms, false, true);
        return Actor;
    }

    // An overlap test only sees inside simple collision, so a canopy volume needs its convex hull.
    bool HasCanopyVolume(const UStaticMesh* Mesh)
    {
        return Mesh && Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0;
    }

    // Hidden foliage volumes, one per tree. They only overlap, so the ball's contact sweeps and ground probes never
    // hit them, while its canopy query slows it inside. A separate actor: obstacle tags on an actor apply to all of it.
    // Names the tree mesh a canopy actor serves, so rebuilding one species' canopy replaces only its own.
    FName CanopyForTag(const UStaticMesh* TreeMesh)
    {
        return FName(*(TEXT("OpenGolf.Terrain.TreeCanopyFor.") + TreeMesh->GetPathName()));
    }

    AActor* MakeTreeCanopy(AOpenGolfTerrainArea* Area, const UStaticMesh* TreeMesh, UStaticMesh* Mesh, const TArray<FTransform>& Transforms)
    {
        auto* Actor = Area->GetWorld()->SpawnActor<AActor>();
        Actor->SetOwner(Area);
        Actor->Tags.Append({AOpenGolfTerrainArea::GeneratedTag, AOpenGolfTerrainArea::TreeCanopyTag, AOpenFairwayBall::GetCanopyTag(), CanopyForTag(TreeMesh)});
        Actor->SetActorLabel(FString::Printf(TEXT("Tree canopy %s"), *TreeMesh->GetName()));
        Actor->SetFolderPath(TEXT("Terrain/Trees"));
        auto* Canopy = NewObject<UInstancedStaticMeshComponent>(Actor, TEXT("CanopyVolumes"), RF_Transactional);
        Canopy->SetMobility(EComponentMobility::Static);
        Canopy->SetStaticMesh(Mesh);
        Canopy->SetCollisionProfileName(TEXT("OverlapAll"));
        Canopy->SetGenerateOverlapEvents(false);
        Canopy->SetVisibility(false);
        Canopy->SetHiddenInGame(true);
        Canopy->SetCastShadow(false);
        Canopy->SetCanEverAffectNavigation(false);
        Canopy->ComponentTags.Add(AOpenFairwayBall::GetCanopyTag());
        Actor->SetRootComponent(Canopy);
        Actor->AddInstanceComponent(Canopy);
        Canopy->RegisterComponent();
        Canopy->AddInstances(Transforms, false, true);
        return Actor;
    }

    // Every check and placement for a vegetation run, before anything in the level changes.
    bool PrepareVegetation(const AOpenGolfTerrainArea* Area, TArray<FScatterJob>& Jobs, FString& Error)
    {
        const auto& D = *Area->Definition;
        Jobs.Reset();
        for (const UOpenGolfVegetationPreset* Preset : D.Vegetation)
            Jobs.Add({Preset->GetName(), Preset, nullptr});
        for (const auto& VegetationArea : D.VegetationAreas)
            Jobs.Add({VegetationArea.Name.ToString(), VegetationArea.Preset, &VegetationArea.OutlineM});
        for (const auto& Job : Jobs)
        {
            if (!HasValidMeshes(*Job.Preset))
            {
                Error = TEXT("Vegetation presets must contain valid meshes.");
                return false;
            }
            if (Job.Preset->Kind == EOpenGolfVegetationKind::Tree && !Job.Preset->TreeCollisionMesh &&
                !LoadObject<UStaticMesh>(nullptr, TrunkColliderMeshPath))
            {
                Error = TEXT("Could not load the trunk collider mesh.");
                return false;
            }
            if (Job.Preset->Kind == EOpenGolfVegetationKind::Tree && Job.Preset->TreeCanopyMesh && !HasCanopyVolume(Job.Preset->TreeCanopyMesh))
            {
                Error = FString::Printf(TEXT("%s: the canopy mesh needs simple (convex) collision."), *Job.Name);
                return false;
            }
        }
        auto Ground = [Area, &D](FVector2D P, FVector& Position, FVector& Normal)
        {
            return Area->GroundAt(Area->GetActorTransform().TransformPosition(FVector(P.X * 100, P.Y * 100, D.HeightAt(P) * 100)), Position, Normal);
        };
        for (auto& Job : Jobs)
        {
            if (!D.Scatter(*Job.Preset, Job.OutlineM, Ground, Job.Points, Error, Area->GetActorRotation().Yaw))
                return false;
            if (Job.Preset->Kind == EOpenGolfVegetationKind::Tree && Job.Points.Num() > MaxTreesPerArea)
            {
                Error = FString::Printf(TEXT("%s places %d trees; the limit is %d. Increase spacing or reduce the area."), *Job.Name, Job.Points.Num(), MaxTreesPerArea);
                return false;
            }
        }
        Jobs = SplitByMeshes(MoveTemp(Jobs));
        return true;
    }

    bool EmitVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, const TArray<FScatterJob>& Jobs, FString& Error)
    {
        const auto& D = *Area->Definition;
        const FString Folder = OutputFolder / (TEXT("Vegetation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        for (int32 Index = 0; Index < Jobs.Num(); ++Index)
        {
            const auto& Preset = *Jobs[Index].Preset;
            const bool bTree = Preset.Kind == EOpenGolfVegetationKind::Tree;
            const TArray<FOpenGolfScatterPoint>& Scattered = Jobs[Index].Points;
            if (Scattered.IsEmpty())
            {
                // PCG would produce nothing to bake, so an empty area gets no actors.
                UE_LOG(LogTemp, Warning, TEXT("OpenGolf vegetation %s placed nothing; check its outline, spacing and filters."), *Jobs[Index].Name);
                continue;
            }
            const FString DataName = Folder / FString::Printf(TEXT("Points_%d"), Index);
            auto* Data = NewObject<UPCGDataAsset>(CreatePackage(*DataName), *FPackageName::GetLongPackageAssetName(DataName), RF_Public | RF_Standalone);
            auto* PointData = NewObject<UPCGPointData>(Data);
            auto& Points = PointData->GetMutablePoints();
            Points.Reserve(Scattered.Num());
            for (const auto& Source : Scattered)
            {
                FPCGPoint& Point = Points.Emplace_GetRef();
                Point.Transform = Source.Transform;
                Point.Density = 1;
                Point.Seed = Source.Seed;
            }
            auto& Tagged = Data->Data.TaggedData.Emplace_GetRef();
            Tagged.Data = PointData;
            Tagged.Pin = TEXT("Out");
            if (!SaveVegetationAsset(Data))
            {
                Error = TEXT("Could not save vegetation point data.");
                return false;
            }
            const FString GraphName = Folder / FString::Printf(TEXT("PCG_Vegetation_%d"), Index);
            auto* Graph = NewObject<UPCGGraph>(CreatePackage(*GraphName), *FPackageName::GetLongPackageAssetName(GraphName), RF_Public | RF_Standalone);
            UPCGLoadDataAssetSettings* LoadSettings = nullptr;
            UPCGNode* LoadNode = Graph->AddNodeOfType(LoadSettings);
            LoadSettings->Asset = Data;
            LoadSettings->UpdateFromData();
            LoadNode->UpdateAfterSettingsChangeDuringCreation();
            UPCGStaticMeshSpawnerSettings* SpawnSettings = nullptr;
            UPCGNode* SpawnNode = Graph->AddNodeOfType(SpawnSettings);
            SpawnSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
            auto* Selector = CastChecked<UPCGMeshSelectorWeighted>(SpawnSettings->MeshSelectorParameters);
            for (UStaticMesh* Mesh : Jobs[Index].Meshes)
            {
                auto& Entry = Selector->MeshEntries.Emplace_GetRef();
                Entry.Descriptor.StaticMesh = Mesh;
                // Trees block the ball through separate trunk colliders, never through their meshes.
                Entry.Descriptor.BodyInstance.SetCollisionProfileName(TEXT("NoCollision"));
                Entry.Descriptor.bCastShadow = bTree;
                Entry.Descriptor.bEvaluateWorldPositionOffset = false;
                // Trees are never culled, so a visible tree always matches its trunk collider.
                Entry.Descriptor.InstanceStartCullDistance = bTree ? 0 : Preset.CullDistanceM * 75;
                Entry.Descriptor.InstanceEndCullDistance = bTree ? 0 : Preset.CullDistanceM * 100;
                Entry.Weight = 1;
            }
            Graph->AddEdge(LoadNode, TEXT("Out"), SpawnNode, TEXT("In"));
            Graph->AddEdge(SpawnNode, TEXT("Out"), Graph->GetOutputNode(), TEXT("Out"));
            if (!SaveVegetationAsset(Graph))
            {
                Error = TEXT("Could not save vegetation graph.");
                return false;
            }
            auto* Actor = Area->GetWorld()->SpawnActor<AActor>();
            Actor->SetOwner(Area);
            Actor->Tags.Add(AOpenGolfTerrainArea::GeneratedTag);
            // Tells BakeVegetation which kind of vegetation this graph produces.
            Actor->Tags.Add(bTree ? AOpenGolfTerrainArea::TreesTag : AOpenGolfTerrainArea::VegetationTag);
            if (Jobs[Index].bCourseGrass)
                Actor->Tags.Append({CourseGrassTag, PendingCourseGrassTag});
            Actor->SetActorLabel(FString::Printf(TEXT("PCG %s"), *Jobs[Index].Name));
            Actor->SetFolderPath(bTree ? TEXT("Terrain/Trees") : TEXT("Terrain/Vegetation"));
            auto* Root = NewObject<UBoxComponent>(Actor, TEXT("Bounds"), RF_Transactional);
            FBox2D Bounds(D.DetailMinimumM, D.DetailMaximumM);
            if (Jobs[Index].OutlineM)
                Bounds = FBox2D(*Jobs[Index].OutlineM);
            const FVector2D Center = Bounds.GetCenter();
            const FVector2D HalfSize = Bounds.GetExtent() * 100;
            Root->SetBoxExtent(FVector(HalfSize.X, HalfSize.Y, 10000));
            Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Root->SetHiddenInGame(true);
            Actor->SetRootComponent(Root);
            Actor->AddInstanceComponent(Root);
            Root->RegisterComponent();
            Actor->SetActorLocation(Area->GetActorTransform().TransformPosition(FVector(Center.X * 100, Center.Y * 100, 0)));
            Actor->SetActorRotation(Area->GetActorRotation());
            auto* PCG = NewObject<UPCGComponent>(Actor, TEXT("Vegetation"), RF_Transactional);
            Actor->AddInstanceComponent(PCG);
            PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
            PCG->RegisterComponent();
            PCG->SetGraph(Graph);
            AActor* Colliders = nullptr;
            AActor* Canopy = nullptr;
            if (bTree)
            {
                Colliders = MakeTreeColliders(Area, Preset, Scattered, Jobs[Index].Name, Error);
                if (!Colliders)
                {
                    Area->GetWorld()->DestroyActor(Actor);
                    return false;
                }
                if (Preset.TreeCanopyMesh && !Jobs[Index].Meshes.IsEmpty())
                {
                    TArray<FTransform> Transforms;
                    Transforms.Reserve(Scattered.Num());
                    for (const auto& Point : Scattered)
                        Transforms.Add(Point.Transform);
                    Canopy = MakeTreeCanopy(Area, Jobs[Index].Meshes[0], Preset.TreeCanopyMesh, Transforms);
                }
            }
            Area->GeneratedActors.Add(Actor);
            if (Colliders)
                Area->GeneratedActors.Add(Colliders);
            if (Canopy)
                Area->GeneratedActors.Add(Canopy);
            UE_LOG(LogTemp, Display, TEXT("OpenGolf vegetation %s: %d %s."), *Jobs[Index].Name, Scattered.Num(), bTree ? TEXT("trees") : TEXT("ground-cover instances"));
            PCG->GenerateLocal(true);
        }
        Area->MarkPackageDirty();
        return true;
    }
}

bool UOpenGolfTerrainBuilder::StartVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error)
{
    Error.Reset();
    if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() || !Area->GenerationId.IsValid())
    {
        Error = TEXT("Build terrain in an editor world before placing vegetation.");
        return false;
    }
    if (!Area->Definition->Validate(Error) || !FPackageName::IsValidLongPackageName(OutputFolder))
        return false;
    for (AActor* Actor : Area->GeneratedActors)
        if (IsValid(Actor) && IsVegetationActor(Actor))
        {
            Error = TEXT("Vegetation already exists for this build. Rebuild the vegetation or the terrain area first.");
            return false;
        }
    TArray<FScatterJob> Jobs;
    return PrepareVegetation(Area, Jobs, Error) && EmitVegetation(Area, OutputFolder, Jobs, Error);
}

bool UOpenGolfTerrainBuilder::RebuildVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error)
{
    Error.Reset();
    if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() || !Area->GenerationId.IsValid())
    {
        Error = TEXT("Build terrain in an editor world before rebuilding vegetation.");
        return false;
    }
    if (!Area->Definition->Validate(Error) || !FPackageName::IsValidLongPackageName(OutputFolder))
        return false;
    for (const AActor* Actor : Area->GeneratedActors)
        if (IsValid(Actor) && IsVegetationActor(Actor) && Actor->GetOwner() != Area)
        {
            Error = TEXT("A vegetation actor has changed ownership. Existing content was preserved.");
            return false;
        }
    // Placement runs first, so a failure leaves the existing vegetation in place.
    TArray<FScatterJob> Jobs;
    if (!PrepareVegetation(Area, Jobs, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("OpenGolf vegetation rebuild failed; existing vegetation kept: %s"), *Error);
        return false;
    }
    Area->Modify();
    int32 Removed = 0;
    for (int32 Index = Area->GeneratedActors.Num() - 1; Index >= 0; --Index)
    {
        AActor* Actor = Area->GeneratedActors[Index];
        if (IsValid(Actor) && !IsVegetationActor(Actor))
            continue;
        if (IsValid(Actor))
        {
            Area->GetWorld()->DestroyActor(Actor);
            ++Removed;
        }
        Area->GeneratedActors.RemoveAt(Index);
    }
    UE_LOG(LogTemp, Display, TEXT("OpenGolf vegetation rebuild removed %d actors; terrain, tee and targets were kept."), Removed);
    if (EmitVegetation(Area, OutputFolder, Jobs, Error))
        return true;
    UE_LOG(LogTemp, Error, TEXT("OpenGolf vegetation rebuild failed: %s"), *Error);
    return false;
}

namespace
{
    bool PlaceCourseGrass(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error)
    {
        Error.Reset();
        if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() ||
            !FPackageName::IsValidLongPackageName(OutputFolder))
        {
            Error = TEXT("Course grass needs an editor terrain area and a valid output folder.");
            return false;
        }
        if (Area->CourseGrassPresets.IsEmpty() && !Area->BoundaryGrassPreset)
        {
            Error = TEXT("Choose course grass presets or a boundary grass preset on the terrain area.");
            return false;
        }
        if (!Area->Definition->Validate(Error))
            return false;
        for (AActor* Actor : Area->GeneratedActors)
        {
            if (IsValid(Actor) && Actor->ActorHasTag(CourseGrassTag) && Actor->GetOwner() != Area)
            {
                Error = TEXT("Course grass ownership changed. Existing content was preserved.");
                return false;
            }
            if (IsValid(Actor) && (Actor->ActorHasTag(PendingCourseGrassTag) || Actor->FindComponentByClass<UPCGComponent>()))
            {
                Error = TEXT("Bake the pending PCG work before rebuilding course grass.");
                return false;
            }
        }
        const auto& D = *Area->Definition;
        auto Ground = [Area, &D](FVector2D XY, FVector& Position, FVector& Normal)
        {
            return Area->GroundAt(Area->GetActorTransform().TransformPosition(FVector(XY.X * 100, XY.Y * 100, D.HeightAt(XY) * 100)), Position, Normal);
        };
        const double Yaw = Area->GetActorRotation().Yaw;
        TArray<FScatterJob> Jobs;
        for (const UOpenGolfVegetationPreset* Preset : Area->CourseGrassPresets)
        {
            if (!Preset || Preset->Kind != EOpenGolfVegetationKind::GroundCover || !HasValidMeshes(*Preset))
            {
                Error = TEXT("Course grass presets must be ground cover with valid meshes.");
                return false;
            }
            FScatterJob& Job = Jobs.AddDefaulted_GetRef();
            Job.Name = Preset->GetName();
            Job.Preset = Preset;
            Job.bCourseGrass = true;
            if (!D.Scatter(*Preset, nullptr, Ground, Job.Points, Error, Yaw))
                return false;
        }
        if (const UOpenGolfVegetationPreset* Preset = Area->BoundaryGrassPreset)
        {
            if (!HasValidMeshes(*Preset) || Preset->PlacementPattern == EOpenGolfPlacementPattern::Tiled)
            {
                Error = TEXT("Boundary grass needs its edge-strip meshes in Meshes.");
                return false;
            }
            FScatterJob& Job = Jobs.AddDefaulted_GetRef();
            Job.Name = Preset->GetName();
            Job.Preset = Preset;
            Job.bCourseGrass = true;
            if (!D.ScatterBoundary(*Preset, Ground, Job.Points, Error, Yaw))
                return false;
        }
        int32 Total = 0;
        for (const auto& Job : Jobs)
            Total += Job.Points.Num();
        if (Total == 0)
        {
            Error = TEXT("No course grass was placed. The existing layer was kept; check the presets and the course outlines.");
            return false;
        }
        UE_LOG(LogTemp, Display, TEXT("OpenGolf course grass: %d instances from %d presets."), Total, Jobs.Num());
        return EmitVegetation(Area, OutputFolder, SplitByMeshes(MoveTemp(Jobs)), Error);
    }
}

bool UOpenGolfTerrainBuilder::RebuildCourseGrass(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error)
{
    Error.Reset();
    if (PlaceCourseGrass(Area, OutputFolder, Error))
        return true;
    UE_LOG(LogTemp, Error, TEXT("OpenGolf course grass failed; the existing layer was kept: %s"), *Error);
    return false;
}

bool UOpenGolfTerrainBuilder::RebuildTreeCanopy(AOpenGolfTerrainArea* Area, UStaticMesh* TreeMesh, UStaticMesh* CanopyMesh, int32& Count, FString& Error)
{
    Error.Reset();
    Count = 0;
    if (!IsValid(Area) || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() || !TreeMesh || !HasCanopyVolume(CanopyMesh))
    {
        Error = TEXT("Tree canopy needs an editor terrain area, the tree mesh and a canopy mesh with simple (convex) collision.");
        UE_LOG(LogTemp, Error, TEXT("OpenGolf tree canopy: %s"), *Error);
        return false;
    }
    TArray<FTransform> Transforms;
    for (const AActor* Actor : Area->GeneratedActors)
    {
        if (!IsValid(Actor) || !Actor->ActorHasTag(AOpenGolfTerrainArea::TreesTag))
            continue;
        if (Actor->FindComponentByClass<UPCGComponent>())
        {
            Error = TEXT("Bake the trees before rebuilding their canopy.");
            UE_LOG(LogTemp, Error, TEXT("OpenGolf tree canopy: %s"), *Error);
            return false;
        }
        TArray<UInstancedStaticMeshComponent*> Components;
        Actor->GetComponents(Components);
        for (const auto* Component : Components)
            if (Component->GetStaticMesh() == TreeMesh)
                for (int32 Instance = 0; Instance < Component->GetInstanceCount(); ++Instance)
                {
                    FTransform Transform;
                    Component->GetInstanceTransform(Instance, Transform, true);
                    Transforms.Add(Transform);
                }
    }
    if (Transforms.IsEmpty())
    {
        Error = FString::Printf(TEXT("No baked trees use %s; the existing canopy was kept."), *TreeMesh->GetName());
        UE_LOG(LogTemp, Error, TEXT("OpenGolf tree canopy: %s"), *Error);
        return false;
    }
    // Only this tree mesh's canopy is replaced; other species keep theirs.
    const FName ForTree = CanopyForTag(TreeMesh);
    Area->Modify();
    for (int32 Index = Area->GeneratedActors.Num() - 1; Index >= 0; --Index)
    {
        AActor* Actor = Area->GeneratedActors[Index];
        if (IsValid(Actor) && Actor->ActorHasTag(AOpenGolfTerrainArea::TreeCanopyTag) && Actor->ActorHasTag(ForTree))
        {
            Area->GetWorld()->DestroyActor(Actor);
            Area->GeneratedActors.RemoveAt(Index);
        }
    }
    Area->GeneratedActors.Add(MakeTreeCanopy(Area, TreeMesh, CanopyMesh, Transforms));
    Area->MarkPackageDirty();
    Count = Transforms.Num();
    UE_LOG(LogTemp, Display, TEXT("OpenGolf tree canopy: %d %s volumes."), Count, *CanopyMesh->GetName());
    return true;
}

AActor* UOpenGolfTerrainBuilder::AddVegetationHost(AOpenGolfTerrainArea* Area, const FString& Label)
{
    if (!IsValid(Area) || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() || Label.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("OpenGolf vegetation host needs an editor terrain area and a label."));
        return nullptr;
    }
    auto* Actor = Area->GetWorld()->SpawnActor<AActor>();
    if (!Actor)
    {
        UE_LOG(LogTemp, Error, TEXT("OpenGolf vegetation host %s could not be spawned."), *Label);
        return nullptr;
    }
    auto* Root = NewObject<USceneComponent>(Actor, TEXT("Root"), RF_Transactional);
    Actor->SetRootComponent(Root);
    Actor->AddInstanceComponent(Root);
    Root->RegisterComponent();
    Actor->SetActorTransform(Area->GetActorTransform());
    Actor->SetOwner(Area);
    Actor->Tags.Append({AOpenGolfTerrainArea::GeneratedTag, AOpenGolfTerrainArea::VegetationTag});
    Actor->SetActorLabel(Label);
    Actor->SetFolderPath(TEXT("Terrain/Vegetation"));
    Area->Modify();
    Area->GeneratedActors.Add(Actor);
    Area->MarkPackageDirty();
    return Actor;
}

bool UOpenGolfTerrainBuilder::BakeVegetation(AOpenGolfTerrainArea* Area, FString& Error)
{
    Error.Reset();
    if (!IsValid(Area) || !Area->GetWorld() || Area->GetWorld()->IsGameWorld())
    {
        Error = TEXT("Vegetation baking requires an editor terrain area.");
        return false;
    }
    for (AActor* Actor : Area->GeneratedActors)
        if (IsValid(Actor))
            if (auto* PCG = Actor->FindComponentByClass<UPCGComponent>(); PCG && PCG->IsGenerating())
            {
                Error = TEXT("PCG is still generating. Wait for completion before baking.");
                return false;
            }
    for (auto& Actor : Area->GeneratedActors)
        if (IsValid(Actor))
            if (auto* PCG = Actor->FindComponentByClass<UPCGComponent>())
            {
                AActor* Baked = PCG->ClearPCGLink();
                if (!Baked)
                {
                    Error = TEXT("PCG produced no bakeable actor. Inspect its graph and logs.");
                    return false;
                }
                const bool bTrees = Actor->ActorHasTag(AOpenGolfTerrainArea::TreesTag);
                Baked->SetOwner(Area);
                Baked->Tags.AddUnique(AOpenGolfTerrainArea::GeneratedTag);
                Baked->Tags.AddUnique(bTrees ? AOpenGolfTerrainArea::TreesTag : AOpenGolfTerrainArea::VegetationTag);
                if (Actor->ActorHasTag(CourseGrassTag))
                    Baked->Tags.Append({CourseGrassTag, PendingCourseGrassTag});
                Baked->SetActorLabel(bTrees ? TEXT("Coastal trees") : TEXT("Coastal vegetation"));
                Baked->SetFolderPath(bTrees ? TEXT("Terrain/Trees") : TEXT("Terrain/Vegetation"));
                TArray<UInstancedStaticMeshComponent*> Components;
                Baked->GetComponents(Components);
                for (auto* Component : Components)
                {
                    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Component->SetCanEverAffectNavigation(false);
                }
                Area->GetWorld()->DestroyActor(Actor);
                Actor = Baked;
            }
    bool HasNewCourseGrass = false;
    for (const AActor* Actor : Area->GeneratedActors)
        HasNewCourseGrass |= IsValid(Actor) && Actor->ActorHasTag(PendingCourseGrassTag);
    // The new layer is baked, so the previous one can go.
    if (HasNewCourseGrass)
        for (int32 I = Area->GeneratedActors.Num() - 1; I >= 0; --I)
        {
            AActor* Actor = Area->GeneratedActors[I];
            if (!IsValid(Actor) || !Actor->ActorHasTag(CourseGrassTag))
                continue;
            if (Actor->ActorHasTag(PendingCourseGrassTag))
            {
                Actor->Tags.Remove(PendingCourseGrassTag);
                Actor->SetActorLabel(TEXT("Course grass"));
            }
            else
            {
                Area->GetWorld()->DestroyActor(Actor);
                Area->GeneratedActors.RemoveAt(I);
            }
        }
    Area->MarkPackageDirty();
    return true;
}
