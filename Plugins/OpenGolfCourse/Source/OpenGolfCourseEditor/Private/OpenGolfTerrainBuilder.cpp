// SPDX-License-Identifier: MIT
#include "OpenGolfTerrainBuilder.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfCourse.h"
#include "OpenFairwayPhysicalMaterial.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CompGeom/Delaunay2.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "UObject/SavePackage.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, OpenGolfCourseEditor)

namespace
{
    using UE::Geometry::FIndex2i;
    using UE::Geometry::FIndex3i;
    const FName GeneratedTag = AOpenGolfTerrainArea::GeneratedTag;

    bool SaveAsset(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Asset,
            *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
    }

    UOpenFairwayPhysicalMaterial* Physical(const UOpenGolfSurfacePreset* Surface)
    {
        const FString Name = StaticEnum<EOpenFairwaySurface>()->GetNameStringByValue(static_cast<int64>(Surface->Surface));
        return LoadObject<UOpenFairwayPhysicalMaterial>(nullptr, *OpenGolfCourse::SurfacePhysicalMaterialPath(Name));
    }

    void OwnActor(AActor* Actor, AOpenGolfTerrainArea* Area, const FString& Label)
    {
        Actor->SetOwner(Area);
        Actor->Tags.AddUnique(GeneratedTag);
        Actor->SetActorLabel(Label);
        Actor->SetFolderPath(FName(*FString::Printf(TEXT("Terrain/%s"), *Area->GetActorLabel())));
    }

    uint16 EncodeHeight(double HeightM)
    {
        return static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(32768 + HeightM * 128), 0, 65535));
    }

    ALandscape* MakeLandscape(AOpenGolfTerrainArea* Area, FVector2D Minimum, FVector2D Maximum, const FString& Label)
    {
        const auto& D = *Area->Definition;
        const int32 NX = FMath::RoundToInt((Maximum.X - Minimum.X) / D.LandscapeSpacingM);
        const int32 NY = FMath::RoundToInt((Maximum.Y - Minimum.Y) / D.LandscapeSpacingM);
        TMap<FGuid, TArray<uint16>> HeightData;
        auto& Heights = HeightData.Add(FGuid());
        Heights.SetNumUninitialized((NX + 1) * (NY + 1));
        for (int32 Y = 0; Y <= NY; ++Y)
            for (int32 X = 0; X <= NX; ++X)
                Heights[Y * (NX + 1) + X] = EncodeHeight(D.HeightAt(Minimum + FVector2D(X, Y) * D.LandscapeSpacingM));
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transactional;
        ALandscape* Land = Area->GetWorld()->SpawnActor<ALandscape>(Params);
        OwnActor(Land, Area, Label);
        Land->SetActorLocation(Area->GetActorTransform().TransformPosition(FVector(Minimum.X * 100, Minimum.Y * 100, 0)));
        Land->SetActorRotation(Area->GetActorRotation());
        Land->SetActorScale3D(FVector(D.LandscapeSpacingM * 100, D.LandscapeSpacingM * 100, 100));
        Land->LandscapeMaterial = D.DefaultSurface->Material;
        Land->DefaultPhysMaterial = Physical(D.DefaultSurface);
        Land->CollisionMipLevel = 0;
        Land->SimpleCollisionMipLevel = 0;
        TMap<FGuid, TArray<FLandscapeImportLayerInfo>> Layers;
        Layers.Add(FGuid());
        Land->Import(FGuid::NewGuid(), 0, 0, NX, NY, 1, 63, HeightData, nullptr, Layers, ELandscapeImportAlphamapType::Additive, {});
        Land->CreateLandscapeInfo();
        Land->RegisterAllComponents();
        Land->UpdateAllComponentMaterialInstances();
        Land->MarkPackageDirty();
        return Land;
    }

    bool Triangulate(const UOpenGolfTerrainDefinition& D, TArray<FVector2D>& Points, TArray<FIndex3i>& Triangles, FString& Error)
    {
        TArray<FIndex2i> Edges;
        auto AddLoop = [&](const TArray<FVector2D>& Outline, double Spacing)
        {
            const int32 Start = Points.Num();
            for (int32 I = 0; I < Outline.Num(); ++I)
            {
                const FVector2D A = Outline[I], B = Outline[(I + 1) % Outline.Num()];
                const int32 Steps = FMath::Max(1, FMath::CeilToInt((B - A).Size() / Spacing));
                for (int32 S = 0; S < Steps; ++S)
                    Points.Add(FMath::Lerp(A, B, static_cast<double>(S) / Steps));
            }
            for (int32 I = Start; I < Points.Num(); ++I)
                Edges.Emplace(I, I + 1 == Points.Num() ? Start : I + 1);
        };
        const FVector2D Min = D.DetailMinimumM, Max = D.DetailMaximumM;
        const TArray<FVector2D> Boundary = {Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y)};
        AddLoop(Boundary, D.LandscapeSpacingM);
        for (const auto& R : D.Regions)
            AddLoop(R.OutlineM, R.SpacingM);
        auto NearEdge = [&](FVector2D P)
        {
            for (const auto& R : D.Regions)
                if (D.DistanceToOutline(R.OutlineM, P) < 0.05)
                    return true;
            return false;
        };
        for (double Y = Min.Y + D.DetailSpacingM * 0.5; Y < Max.Y; Y += D.DetailSpacingM)
            for (double X = Min.X + D.DetailSpacingM * 0.5; X < Max.X; X += D.DetailSpacingM)
            {
                const FVector2D P(X, Y);
                bool Fine = false;
                for (const auto& R : D.Regions)
                    Fine |= R.SpacingM < D.DetailSpacingM && D.Contains(R.OutlineM, P);
                if (!Fine && !NearEdge(P))
                    Points.Add(P);
            }
        for (int32 RI = 0; RI < D.Regions.Num(); ++RI)
        {
            const auto& R = D.Regions[RI];
            if (R.SpacingM >= D.DetailSpacingM)
                continue;
            FBox2D Box(R.OutlineM);
            for (double Y = Box.Min.Y + R.SpacingM * 0.5; Y < Box.Max.Y; Y += R.SpacingM)
                for (double X = Box.Min.X + R.SpacingM * 0.5; X < Box.Max.X; X += R.SpacingM)
                {
                    const FVector2D P(X, Y);
                    if (!D.Contains(R.OutlineM, P) || NearEdge(P))
                        continue;
                    bool Finer = false;
                    for (int32 Other = 0; Other < D.Regions.Num(); ++Other)
                        if (Other != RI && (D.Regions[Other].SpacingM < R.SpacingM || (D.Regions[Other].SpacingM == R.SpacingM && Other > RI)))
                            Finer |= D.Contains(D.Regions[Other].OutlineM, P);
                    if (!Finer)
                        Points.Add(P);
                    if (Points.Num() > 200000)
                    {
                        Error = TEXT("Detailed terrain exceeds 200,000 vertices; increase sample spacing.");
                        return false;
                    }
                }
        }
        UE::Geometry::FDelaunay2 DT;
        DT.bAutomaticallyFixEdgesToDuplicateVertices = true;
        if (!DT.Triangulate(Points, Edges))
        {
            Error = TEXT("Could not triangulate terrain. Check touching or crossing region outlines.");
            return false;
        }
        Triangles = DT.GetTriangles();
        return !Triangles.IsEmpty();
    }

    UStaticMesh* MakeMesh(const UOpenGolfTerrainDefinition& D, UOpenGolfSurfacePreset* Preset, const TArray<FVector2D>& Points,
        const TArray<FIndex3i>& Triangles, const FString& PackageName)
    {
        FMeshDescription Description;
        FStaticMeshAttributes Attributes(Description);
        Attributes.Register();
        auto Positions = Attributes.GetVertexPositions();
        auto Normals = Attributes.GetVertexInstanceNormals();
        auto Tangents = Attributes.GetVertexInstanceTangents();
        auto Signs = Attributes.GetVertexInstanceBinormalSigns();
        auto UVs = Attributes.GetVertexInstanceUVs();
        UVs.SetNumChannels(1);
        const FPolygonGroupID Group = Description.CreatePolygonGroup();
        Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Surface");
        TMap<int32, FVertexID> Vertices;
        for (auto T : Triangles)
        {
            const FVector2D P = (Points[T.A] + Points[T.B] + Points[T.C]) / 3;
            if (D.SurfaceAt(P) != Preset)
                continue;
            const FVector2D AB = Points[T.B] - Points[T.A], AC = Points[T.C] - Points[T.A];
            if (AB.X * AC.Y - AB.Y * AC.X > 0)
                Swap(T.B, T.C);
            TArray<FVertexInstanceID> Corners;
            for (int32 Index : {T.A, T.B, T.C})
            {
                const FVector2D V = Points[Index];
                FVertexID* Found = Vertices.Find(Index);
                FVertexID ID;
                if (Found)
                    ID = *Found;
                else
                {
                    ID = Description.CreateVertex();
                    Vertices.Add(Index, ID);
                    double Z = D.HeightAt(V);
                    if (FMath::IsNearlyEqual(V.X, D.DetailMinimumM.X) || FMath::IsNearlyEqual(V.X, D.DetailMaximumM.X) ||
                        FMath::IsNearlyEqual(V.Y, D.DetailMinimumM.Y) || FMath::IsNearlyEqual(V.Y, D.DetailMaximumM.Y))
                        Z = (static_cast<int32>(EncodeHeight(Z)) - 32768) / 128.0;
                    Positions[ID] = FVector3f(V.X * 100, V.Y * 100, Z * 100);
                }
                const FVertexInstanceID VI = Description.CreateVertexInstance(ID);
                const FVector N = D.NormalAt(V);
                Normals[VI] = FVector3f(N);
                Tangents[VI] = FVector3f((FVector::ForwardVector - N * N.X).GetSafeNormal());
                Signs[VI] = 1;
                UVs.Set(VI, 0, FVector2f(V));
                Corners.Add(VI);
            }
            Description.CreatePolygon(Group, Corners);
        }
        if (Description.Triangles().Num() == 0)
            return nullptr;
        UPackage* Package = CreatePackage(*PackageName);
        auto* Mesh = NewObject<UStaticMesh>(Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Preset->Material, TEXT("Surface")));
        UStaticMesh::FBuildMeshDescriptionsParams Params;
        Params.bBuildSimpleCollision = false;
        Params.bFastBuild = false;
        Mesh->BuildFromMeshDescriptions({&Description}, Params);
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
        Mesh->CreateBodySetup();
        Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
        Mesh->GetBodySetup()->PhysMaterial = Physical(Preset);
        Mesh->GetBodySetup()->InvalidatePhysicsData();
        Mesh->GetBodySetup()->CreatePhysicsMeshes();
        return SaveAsset(Mesh) ? Mesh : nullptr;
    }
}

bool UOpenGolfTerrainBuilder::BuildTerrain(AOpenGolfTerrainArea* Area, const FString& OutputFolder, bool ReplaceGenerated, FString& Error)
{
    Error.Reset();
    if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld())
    {
        Error = TEXT("Select a terrain area with a definition in an editor world.");
        return false;
    }
    if (!Area->Definition->Validate(Error))
        return false;
    if (!FPackageName::IsValidLongPackageName(OutputFolder) || !Area->GetActorScale3D().Equals(FVector::OneVector) ||
        !FMath::IsNearlyZero(Area->GetActorRotation().Pitch) || !FMath::IsNearlyZero(Area->GetActorRotation().Roll))
    {
        Error = TEXT("Use a valid content folder, unit scale, and yaw-only terrain rotation.");
        return false;
    }
    if (!Area->GeneratedActors.IsEmpty() && !ReplaceGenerated)
    {
        Error = TEXT("This area already has generated actors. Explicitly select replacement to rebuild it.");
        return false;
    }
    for (const AActor* Actor : Area->GeneratedActors)
        if (IsValid(Actor) && (Actor->GetOwner() != Area || !Actor->ActorHasTag(GeneratedTag)))
        {
            Error = TEXT("A generated actor has changed ownership. Existing content was preserved.");
            return false;
        }
    TArray<UOpenGolfSurfacePreset*> Surfaces = {Area->Definition->DefaultSurface};
    for (const auto& R : Area->Definition->Regions)
        Surfaces.AddUnique(R.Surface);
    for (const auto* Surface : Surfaces)
        if (!Surface || !Surface->Material || !Physical(Surface))
        {
            Error = TEXT("All surface presets need a material and an existing OpenFairway physical material.");
            return false;
        }
    TArray<FVector2D> Points;
    TArray<FIndex3i> Triangles;
    if (!Triangulate(*Area->Definition, Points, Triangles, Error))
        return false;
    const FGuid NewId = FGuid::NewGuid();
    const FString Folder = OutputFolder / (TEXT("Build_") + NewId.ToString(EGuidFormats::Digits));
    TArray<TPair<UOpenGolfSurfacePreset*, UStaticMesh*>> Meshes;
    for (int32 I = 0; I < Surfaces.Num(); ++I)
    {
        auto* Mesh = MakeMesh(*Area->Definition, Surfaces[I], Points, Triangles, Folder / FString::Printf(TEXT("SM_Surface_%d"), I));
        if (!Mesh)
        {
            Error = TEXT("A surface mesh could not be built or saved. The existing terrain was preserved.");
            return false;
        }
        Meshes.Emplace(Surfaces[I], Mesh);
    }
    TArray<TObjectPtr<AActor>> Created;
    for (auto [Preset, Mesh] : Meshes)
    {
        auto* Actor = Area->GetWorld()->SpawnActor<AStaticMeshActor>();
        OwnActor(Actor, Area, FString::Printf(TEXT("Terrain %s"), *Preset->GetName()));
        Actor->SetActorTransform(Area->GetActorTransform());
        auto* Component = Actor->GetStaticMeshComponent();
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetPhysMaterialOverride(Physical(Preset));
        Component->SetCanEverAffectNavigation(false);
        Created.Add(Actor);
    }
    const auto& D = *Area->Definition;
    Created.Add(MakeLandscape(Area, D.MinimumM, FVector2D(D.DetailMinimumM.X, D.MaximumM.Y), TEXT("Coastal ground west")));
    Created.Add(MakeLandscape(Area, FVector2D(D.DetailMaximumM.X, D.MinimumM.Y), D.MaximumM, TEXT("Coastal ground east")));
    Created.Add(MakeLandscape(Area, FVector2D(D.DetailMinimumM.X, D.MinimumM.Y), FVector2D(D.DetailMaximumM.X, D.DetailMinimumM.Y), TEXT("Coastal ground south")));
    Created.Add(MakeLandscape(Area, FVector2D(D.DetailMinimumM.X, D.DetailMaximumM.Y), FVector2D(D.DetailMaximumM.X, D.MaximumM.Y), TEXT("Coastal ground north")));
    Area->Modify();
    for (AActor* Actor : Area->GeneratedActors)
        if (IsValid(Actor))
            Area->GetWorld()->DestroyActor(Actor);
    Area->GeneratedActors = MoveTemp(Created);
    Area->GenerationId = NewId;
    Area->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("OpenGolf terrain built: %d vertices, %d triangles, %d actors. Save the level to keep the result."), Points.Num(), Triangles.Num(), Area->GeneratedActors.Num());
    return true;
}

int32 UOpenGolfTerrainBuilder::SurfaceMeshIndex(const AActor& Actor)
{
    const auto* Ground = Cast<AStaticMeshActor>(&Actor);
    if (!Ground || !Actor.ActorHasTag(GeneratedTag) || Actor.ActorHasTag(AOpenGolfTerrainArea::VegetationTag))
        return INDEX_NONE;
    const UStaticMesh* Mesh = Ground->GetStaticMeshComponent()->GetStaticMesh();
    const FString Prefix = TEXT("SM_Surface_");
    const FString Name = Mesh ? Mesh->GetName() : FString();
    const FString Digits = Name.RightChop(Prefix.Len());
    if (!Name.StartsWith(Prefix) || Digits.IsEmpty() || !Digits.IsNumeric() || Digits.Contains(TEXT(".")))
        return INDEX_NONE;
    return FCString::Atoi(*Digits);
}

/** One generated ground actor per surface index, or false when the ground no longer matches the surfaces. */
static bool FindGroundActors(const AOpenGolfTerrainArea& Area, int32 Count, TArray<AStaticMeshActor*>& Grounds, FString& Error)
{
    Grounds.Init(nullptr, Count);
    for (AActor* Actor : Area.GeneratedActors)
    {
        const int32 Index = IsValid(Actor) && Actor->GetOwner() == &Area ? UOpenGolfTerrainBuilder::SurfaceMeshIndex(*Actor) : INDEX_NONE;
        if (Index == INDEX_NONE)
            continue;
        if (!Grounds.IsValidIndex(Index) || Grounds[Index])
        {
            Error = TEXT("The generated ground no longer matches the definition's surfaces. Rebuild the whole terrain instead.");
            return false;
        }
        Grounds[Index] = Cast<AStaticMeshActor>(Actor);
    }
    if (Grounds.Contains(nullptr))
    {
        Error = TEXT("A surface has no generated ground actor. Rebuild the whole terrain instead.");
        return false;
    }
    return true;
}

static bool ReplaceSurfaceMeshes(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error)
{
    if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld())
    {
        Error = TEXT("Select a terrain area with a definition in an editor world.");
        return false;
    }
    if (!Area->Definition->Validate(Error))
        return false;
    if (!FPackageName::IsValidLongPackageName(OutputFolder))
    {
        Error = TEXT("Use a valid content folder.");
        return false;
    }
    // Same order as BuildTerrain, which named each mesh after its index here.
    TArray<UOpenGolfSurfacePreset*> Surfaces = {Area->Definition->DefaultSurface};
    for (const auto& R : Area->Definition->Regions)
        Surfaces.AddUnique(R.Surface);
    for (const auto* Surface : Surfaces)
        if (!Surface || !Surface->Material || !Physical(Surface))
        {
            Error = TEXT("All surface presets need a material and an existing OpenFairway physical material.");
            return false;
        }
    TArray<AStaticMeshActor*> Grounds;
    if (!FindGroundActors(*Area, Surfaces.Num(), Grounds, Error))
        return false;
    TArray<FVector2D> Points;
    TArray<FIndex3i> Triangles;
    if (!Triangulate(*Area->Definition, Points, Triangles, Error))
        return false;
    const FString Folder = OutputFolder / (TEXT("Build_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    TArray<UStaticMesh*> Meshes;
    for (int32 I = 0; I < Surfaces.Num(); ++I)
    {
        UStaticMesh* Mesh = MakeMesh(*Area->Definition, Surfaces[I], Points, Triangles, Folder / FString::Printf(TEXT("SM_Surface_%d"), I));
        if (!Mesh)
        {
            Error = TEXT("A surface mesh could not be built or saved. The existing ground was preserved.");
            return false;
        }
        Meshes.Add(Mesh);
    }
    for (int32 I = 0; I < Surfaces.Num(); ++I)
    {
        auto* Component = Grounds[I]->GetStaticMeshComponent();
        Component->Modify();
        Component->SetStaticMesh(Meshes[I]);
        Component->SetPhysMaterialOverride(Physical(Surfaces[I]));
    }
    Area->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("OpenGolf surfaces rebuilt in %s: %d vertices, %d triangles. Save the level to keep the result."), *Folder, Points.Num(), Triangles.Num());
    return true;
}

bool UOpenGolfTerrainBuilder::RebuildSurfaceMeshes(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error)
{
    Error.Reset();
    if (ReplaceSurfaceMeshes(Area, OutputFolder, Error))
        return true;
    UE_LOG(LogTemp, Error, TEXT("OpenGolf surface rebuild failed; the existing ground was kept: %s"), *Error);
    return false;
}
