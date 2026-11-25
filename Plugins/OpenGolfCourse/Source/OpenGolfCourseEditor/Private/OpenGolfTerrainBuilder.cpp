// SPDX-License-Identifier: MIT
#include "OpenGolfTerrainBuilder.h"
#include "OpenGolfTerrainTiles.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "ScopedTransaction.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfAssetNames.h"
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

    /** The preset's OpenFairway surface by name, such as "Rough": names its ground meshes, actors and physical material. */
    FString SurfaceName(const UOpenGolfSurfacePreset* Surface)
    {
        return StaticEnum<EOpenFairwaySurface>()->GetNameStringByValue(static_cast<int64>(Surface->Surface));
    }

    UOpenFairwayPhysicalMaterial* Physical(const UOpenGolfSurfacePreset* Surface)
    {
        return LoadObject<UOpenFairwayPhysicalMaterial>(nullptr, *OpenGolfCourse::SurfacePhysicalMaterialPath(SurfaceName(Surface)));
    }

    /** Owns, tags, labels and files a generated actor (Docs/ContentNaming.md#actors). */
    void OwnActor(AActor* Actor, AOpenGolfTerrainArea* Area, const FString& Label, const FString& Folder)
    {
        Actor->SetOwner(Area);
        Actor->Tags.AddUnique(GeneratedTag);
        Actor->SetActorLabel(Label);
        Actor->SetFolderPath(FName(*Folder));
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
        OwnActor(Land, Area, Label, TEXT("Terrain/Landscape"));
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

    /**
     * A ground mesh for Preset. The detail area keeps only triangles on Preset and matches the Landscape's height steps
     * at its border; a course square (bTile) passes triangles already sorted by surface and has no Landscape beside it.
     */
    UStaticMesh* MakeMesh(const UOpenGolfTerrainDefinition& D, UOpenGolfSurfacePreset* Preset, const TArray<FVector2D>& Points,
        const TArray<FIndex3i>& Triangles, const FString& PackageName, bool bTile = false)
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
            if (!bTile && D.SurfaceAt(P) != Preset)
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
                    if (!bTile && (FMath::IsNearlyEqual(V.X, D.DetailMinimumM.X) || FMath::IsNearlyEqual(V.X, D.DetailMaximumM.X) ||
                        FMath::IsNearlyEqual(V.Y, D.DetailMinimumM.Y) || FMath::IsNearlyEqual(V.Y, D.DetailMaximumM.Y)))
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
        if (bTile)
        {
            // Keep the elevation grid's normals, which agree along every square and surface edge; recomputed normals
            // would only see one side. Course lighting is dynamic, so no lightmap UVs either.
            FMeshBuildSettings& Settings = Mesh->AddSourceModel().BuildSettings;
            Settings.bRecomputeNormals = false;
            Settings.bRecomputeTangents = false;
            Settings.bGenerateLightmapUVs = false;
        }
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
        auto* Mesh = MakeMesh(*Area->Definition, Surfaces[I], Points, Triangles, Folder / OpenGolfAssetNames::SurfaceMesh(I));
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
        OwnActor(Actor, Area, OpenGolfAssetNames::GroundLabel(SurfaceName(Preset)), OpenGolfAssetNames::SurfaceFolder(SurfaceName(Preset)));
        Actor->SetActorTransform(Area->GetActorTransform());
        auto* Component = Actor->GetStaticMeshComponent();
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetPhysMaterialOverride(Physical(Preset));
        Component->SetCanEverAffectNavigation(false);
        Created.Add(Actor);
    }
    const auto& D = *Area->Definition;
    Created.Add(MakeLandscape(Area, D.MinimumM, FVector2D(D.DetailMinimumM.X, D.MaximumM.Y), OpenGolfAssetNames::KindLabel(TEXT("Landscape"), TEXT("West"))));
    Created.Add(MakeLandscape(Area, FVector2D(D.DetailMaximumM.X, D.MinimumM.Y), D.MaximumM, OpenGolfAssetNames::KindLabel(TEXT("Landscape"), TEXT("East"))));
    Created.Add(MakeLandscape(Area, FVector2D(D.DetailMinimumM.X, D.MinimumM.Y), FVector2D(D.DetailMaximumM.X, D.DetailMinimumM.Y), OpenGolfAssetNames::KindLabel(TEXT("Landscape"), TEXT("South"))));
    Created.Add(MakeLandscape(Area, FVector2D(D.DetailMinimumM.X, D.DetailMaximumM.Y), FVector2D(D.DetailMaximumM.X, D.MaximumM.Y), OpenGolfAssetNames::KindLabel(TEXT("Landscape"), TEXT("North"))));
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
        UStaticMesh* Mesh = MakeMesh(*Area->Definition, Surfaces[I], Points, Triangles, Folder / OpenGolfAssetNames::SurfaceMesh(I));
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

bool UOpenGolfTerrainBuilder::ImportElevation(UOpenGolfTerrainDefinition* Definition, const FString& Filename, FString& Error)
{
    TArray<uint8> Bytes;
    if (!Definition || IFileManager::Get().FileSize(*Filename) > 64000036 ||
        !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() < 36 || FMemory::Memcmp(Bytes.GetData(), "OGE1", 4))
    {
        Error = TEXT("Select a terrain definition and a prepared course elevation file.");
        return false;
    }
    FMemoryReader Reader(Bytes);
    Reader.Seek(4);
    FIntPoint Size;
    double X, Y, Spacing;
    Reader << Size.X << Size.Y << X << Y << Spacing;
    if (Size.X < 2 || Size.Y < 2 || int64(Size.X) * Size.Y != (Bytes.Num() - 36) / 4 || (Bytes.Num() - 36) % 4)
    {
        Error = TEXT("The elevation grid has the wrong size.");
        return false;
    }
    auto* Candidate = DuplicateObject<UOpenGolfTerrainDefinition>(Definition, GetTransientPackage());
    Candidate->ElevationSize = Size;
    Candidate->ElevationOriginM = FVector2D(X, Y);
    Candidate->ElevationSpacingM = Spacing;
    Candidate->ElevationM.SetNumUninitialized(Size.X * Size.Y);
    Reader.Serialize(Candidate->ElevationM.GetData(), Bytes.Num() - 36);
    if (!Candidate->ValidateElevation(Error))
        return false;
    FScopedTransaction Transaction(NSLOCTEXT("OpenGolfCourse", "ImportElevation", "Import course elevation"));
    Definition->Modify();
    Definition->ElevationSize = Size;
    Definition->ElevationOriginM = Candidate->ElevationOriginM;
    Definition->ElevationSpacingM = Candidate->ElevationSpacingM;
    Definition->ElevationM = MoveTemp(Candidate->ElevationM);
    Definition->PostEditChange();
    return true;
}

namespace
{
    /** Destroys the area's generated actors tagged Tag, which a rebuild replaces, and forgets any already gone. */
    void RemoveTagged(AOpenGolfTerrainArea* Area, FName Tag)
    {
        Area->GeneratedActors.RemoveAll([Area, Tag](AActor* Actor)
        {
            if (!IsValid(Actor)) return true;
            if (Actor->GetOwner() != Area || !Actor->ActorHasTag(Tag)) return false;
            Area->GetWorld()->DestroyActor(Actor);
            return true;
        });
    }

    /** A ground actor showing Mesh, labelled with the mesh's name and keyed for rebuilds by Tag. */
    void SpawnGround(AOpenGolfTerrainArea* Area, UStaticMesh* Mesh, UOpenGolfSurfacePreset* Preset, FName Tag, bool bCollision, const FString& Folder)
    {
        auto* Actor = Area->GetWorld()->SpawnActor<AStaticMeshActor>();
        OwnActor(Actor, Area, Mesh->GetName(), Folder);
        Actor->Tags.Add(Tag);
        Actor->SetActorTransform(Area->GetActorTransform());
        auto* Component = Actor->GetStaticMeshComponent();
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
        Component->SetCanEverAffectNavigation(false);
        if (Preset)
            Component->SetPhysMaterialOverride(Physical(Preset));
        else
            Component->SetCastShadow(false);
        Area->GeneratedActors.Add(Actor);
    }
}

bool UOpenGolfTerrainBuilder::BuildCourseTile(AOpenGolfTerrainArea* Area, FIntPoint Tile, const FString& OutputFolder, FString& Error)
{
    if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() ||
        !FPackageName::IsValidLongPackageName(OutputFolder))
    {
        Error = TEXT("Select an editor terrain area with a definition and a valid content folder.");
        return false;
    }
    OpenGolfTerrainTiles::FTile Ground;
    if (!OpenGolfTerrainTiles::Triangulate(*Area->Definition, Tile, Ground, Error))
        return false;
    TMap<UOpenGolfSurfacePreset*, TArray<FIndex3i>> Groups;
    for (int32 I = 0; I < Ground.Triangles.Num(); ++I)
        Groups.FindOrAdd(Ground.Surfaces[I]).Add(Ground.Triangles[I]);
    const FName Tag(*OpenGolfAssetNames::TileTag(Tile));
    // A new folder per build: the square's previous meshes stay intact until the map stops using them.
    const FString Folder = OutputFolder / OpenGolfAssetNames::TileFolder(Tile, FGuid::NewGuid());
    TArray<TPair<UOpenGolfSurfacePreset*, UStaticMesh*>> Built;
    TSet<FString> Names;
    for (const auto& [Preset, Triangles] : Groups)
    {
        if (!Preset || !Preset->Material || !Physical(Preset))
        {
            Error = TEXT("A tile surface is missing its visual or physical material.");
            return false;
        }
        // Two presets of one surface in a square (a tee and a fairway, say) get variants: SM_Fairway_Tile_0_0_02.
        FString Name = OpenGolfAssetNames::TileMesh(SurfaceName(Preset), Tile);
        for (int32 Number = 2; Names.Contains(Name); ++Number)
            Name = OpenGolfAssetNames::TileMesh(SurfaceName(Preset), Tile) + TEXT("_") + OpenGolfAssetNames::Variant(Number);
        Names.Add(Name);
        auto* Mesh = MakeMesh(*Area->Definition, Preset, Ground.Points, Triangles, Folder / Name, true);
        if (!Mesh)
        {
            Error = TEXT("Could not save the new tile meshes.");
            return false;
        }
        Built.Emplace(Preset, Mesh);
    }
    Area->Modify();
    RemoveTagged(Area, Tag);
    for (const auto& [Preset, Mesh] : Built)
        SpawnGround(Area, Mesh, Preset, Tag, true, OpenGolfAssetNames::SurfaceFolder(SurfaceName(Preset)));
    Area->MarkPackageDirty();
    return true;
}

bool UOpenGolfTerrainBuilder::BuildWaterSurface(AOpenGolfTerrainArea* Area, const TArray<FVector2D>& OutlineM, UMaterialInterface* Material,
    const FString& OutputFolder, const FString& Name, FString& Error)
{
    FText NameProblem;
    if (!IsValid(Area) || !Area->Definition || !Area->GetWorld() || Area->GetWorld()->IsGameWorld() || !Material ||
        OutlineM.Num() < 3 || !FPackageName::IsValidLongPackageName(OutputFolder) || Name.IsEmpty() || !FName(*Name).IsValidObjectName(NameProblem))
    {
        Error = TEXT("Water needs an editor terrain area, a material, an outline and a valid content folder and name.");
        return false;
    }
    const auto& D = *Area->Definition;
    // Lidar ground models flatten lakes to one surface height; the median inside the outline ignores the banks.
    TArray<FVector2D> Points;
    TArray<FIndex2i> Edges;
    for (int32 I = 0; I < OutlineM.Num(); ++I)
    {
        const FVector2D A = OutlineM[I], B = OutlineM[(I + 1) % OutlineM.Num()];
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 2));
        for (int32 K = 0; K < Steps; ++K)
            Points.Add(FMath::Lerp(A, B, static_cast<double>(K) / Steps));
    }
    for (int32 I = 0; I < Points.Num(); ++I)
        Edges.Emplace(I, (I + 1) % Points.Num());
    TArray<double> Heights;
    const FBox2D Box(OutlineM);
    for (double Y = Box.Min.Y + 2; Y < Box.Max.Y; Y += 4)
        for (double X = Box.Min.X + 2; X < Box.Max.X; X += 4)
            if (UOpenGolfTerrainDefinition::Contains(OutlineM, FVector2D(X, Y)) && UOpenGolfTerrainDefinition::DistanceToOutline(OutlineM, FVector2D(X, Y)) > 1)
            {
                Points.Emplace(X, Y);
                Heights.Add(D.HeightAt(FVector2D(X, Y)));
            }
    if (Heights.IsEmpty())
    {
        Error = TEXT("The water outline is too small to find its surface height.");
        return false;
    }
    Heights.Sort();
    const float LevelCm = static_cast<float>((Heights[Heights.Num() / 2] + 0.05) * 100);
    UE::Geometry::FDelaunay2 Delaunay;
    Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = true;
    if (!Delaunay.Triangulate(Points, Edges))
    {
        Error = TEXT("Could not triangulate the water outline.");
        return false;
    }
    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    const FPolygonGroupID Group = Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Water");
    TMap<int32, FVertexID> Vertices;
    for (FIndex3i T : Delaunay.GetTriangles())
    {
        if (!UOpenGolfTerrainDefinition::Contains(OutlineM, (Points[T.A] + Points[T.B] + Points[T.C]) / 3))
            continue;
        const FVector2D AB = Points[T.B] - Points[T.A], AC = Points[T.C] - Points[T.A];
        if (AB.X * AC.Y - AB.Y * AC.X > 0)
            Swap(T.B, T.C);
        TArray<FVertexInstanceID> Corners;
        for (int32 Index : {T.A, T.B, T.C})
        {
            FVertexID ID;
            if (const FVertexID* Found = Vertices.Find(Index))
                ID = *Found;
            else
            {
                ID = Vertices.Add(Index, Description.CreateVertex());
                Attributes.GetVertexPositions()[ID] = FVector3f(Points[Index].X * 100, Points[Index].Y * 100, LevelCm);
            }
            const FVertexInstanceID VI = Description.CreateVertexInstance(ID);
            Attributes.GetVertexInstanceNormals()[VI] = FVector3f::UpVector;
            Attributes.GetVertexInstanceTangents()[VI] = FVector3f::ForwardVector;
            Attributes.GetVertexInstanceBinormalSigns()[VI] = 1;
            Attributes.GetVertexInstanceUVs().Set(VI, 0, FVector2f(Points[Index] / 10));
            Corners.Add(VI);
        }
        Description.CreatePolygon(Group, Corners);
    }
    const FString PackageName = OutputFolder / OpenGolfAssetNames::WaterFolder(FGuid::NewGuid()) / OpenGolfAssetNames::FeatureMesh(Name);
    UPackage* Package = CreatePackage(*PackageName);
    auto* Mesh = NewObject<UStaticMesh>(Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Water")));
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Mesh->BuildFromMeshDescriptions({&Description}, Params);
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    if (Description.Triangles().Num() == 0 || !SaveAsset(Mesh))
    {
        Error = TEXT("Could not save the water mesh.");
        return false;
    }
    const FName Tag(*(TEXT("Water_") + Name));
    Area->Modify();
    RemoveTagged(Area, Tag);
    SpawnGround(Area, Mesh, nullptr, Tag, false, TEXT("Water"));
    Area->MarkPackageDirty();
    return true;
}
