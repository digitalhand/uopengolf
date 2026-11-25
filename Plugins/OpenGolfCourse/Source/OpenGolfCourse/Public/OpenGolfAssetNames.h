// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * Names the course tools give generated assets, actors and folders (Docs/ContentNaming.md). The Python generators
 * use the same rules from Plugins/OpenGolfCourse/Scripts/asset_naming.py; keep the two in step.
 *
 * Assets follow [Prefix]_[AssetName]_[Descriptor]_[Variant] with two-digit variants. A placed actor that shows one
 * asset carries that asset's name; any other actor is labelled <Kind>_<Descriptor>, such as Landscape_West.
 */
namespace OpenGolfAssetNames
{
    /** Folder under a course's content folder that holds everything the builder makes. */
    inline const TCHAR* GeneratedFolder = TEXT("Generated");

    /** Two-digit variant: 1 -> "01". */
    inline FString Variant(int32 Number)
    {
        return FString::Printf(TEXT("%02d"), Number);
    }

    /** PascalCase segments from free text, keeping underscores: "turf fairway 0.5m" -> "TurfFairway05m", "SavannaTree_Near_00" stays. */
    inline FString Segment(const FString& Text)
    {
        FString Result;
        bool bWordStart = true;
        for (const TCHAR Char : Text)
        {
            if (Char == TEXT('_'))
            {
                if (!Result.IsEmpty() && !Result.EndsWith(TEXT("_")))
                    Result.AppendChar(Char);
                bWordStart = true;
                continue;
            }
            if (!FChar::IsAlnum(Char))
            {
                bWordStart = true;
                continue;
            }
            Result.AppendChar(bWordStart ? FChar::ToUpper(Char) : Char);
            bWordStart = false;
        }
        Result.RemoveFromEnd(TEXT("_"));
        return Result;
    }

    /** Name without its type prefix: DA_SavannaTreeNear00 -> SavannaTreeNear00. */
    inline FString WithoutPrefix(const FString& Name)
    {
        int32 Underscore = INDEX_NONE;
        if (!Name.FindChar(TEXT('_'), Underscore) || Underscore < 1 || Underscore > 4)
            return Name;
        for (int32 I = 0; I < Underscore; ++I)
            if (!FChar::IsUpper(Name[I]))
                return Name;
        return Name.RightChop(Underscore + 1);
    }

    /** Actor tag that keys a course ground square's rebuild. */
    inline FString TileTag(FIntPoint Tile)
    {
        return FString::Printf(TEXT("Tile_%d_%d"), Tile.X, Tile.Y);
    }

    /** One build of a ground square: Tile_<x>_<y>_<GUID>. A new folder per build keeps the previous meshes intact. */
    inline FString TileFolder(FIntPoint Tile, const FGuid& Build)
    {
        return TileTag(Tile) + TEXT("_") + Build.ToString(EGuidFormats::Digits);
    }

    /** A ground square's mesh for one surface, and the label of the actor showing it: SM_Rough_Tile_0_-10. */
    inline FString TileMesh(const FString& Surface, FIntPoint Tile)
    {
        return FString::Printf(TEXT("SM_%s_Tile_%d_%d"), *Surface, Tile.X, Tile.Y);
    }

    /** One build of a lake: Water_<GUID>. */
    inline FString WaterFolder(const FGuid& Build)
    {
        return TEXT("Water_") + Build.ToString(EGuidFormats::Digits);
    }

    /** A static mesh named after a feature such as Lake_01: SM_Lake_01. */
    inline FString FeatureMesh(const FString& Feature)
    {
        return TEXT("SM_") + Feature;
    }

    /** Whole-area ground mesh number Index of a terrain build: SM_Surface_00. */
    inline FString SurfaceMesh(int32 Index)
    {
        return TEXT("SM_Surface_") + Variant(Index);
    }

    /** Label of a whole-area ground actor for a surface: SM_Fairway_Ground. */
    inline FString GroundLabel(const FString& Surface)
    {
        return FString::Printf(TEXT("SM_%s_Ground"), *Surface);
    }

    /** Outliner folder for the ground of one surface: Terrain/Rough. */
    inline FString SurfaceFolder(const FString& Surface)
    {
        return TEXT("Terrain/") + Surface;
    }

    /** Label of an actor with no asset of its own: <Kind>_<Descriptor>, or <Kind> alone. */
    inline FString KindLabel(const TCHAR* Kind, const FString& Descriptor = FString())
    {
        const FString Clean = Segment(Descriptor);
        return Clean.IsEmpty() ? FString(Kind) : FString::Printf(TEXT("%s_%s"), Kind, *Clean);
    }
}
