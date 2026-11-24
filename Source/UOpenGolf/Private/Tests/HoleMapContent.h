// SPDX-License-Identifier: MIT
// Checks what the range's hole map shows, for the rendered minimap and course tests.
#pragma once

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/TextureRenderTarget2D.h"
#include "Misc/AutomationTest.h"
#include "UnrealClient.h"

namespace HoleMapContent
{
    /**
     * Whether Picture shows turf along the hole: map up is tee to cup, so the fairway runs up the middle. A capture
     * taken while textures were still loading reads grey there (the placeholder checker), and bare sand reads red.
     */
    inline bool ShowsTurf(FAutomationTestBase& Test, UTextureRenderTarget2D* Picture, const FString& Where)
    {
        TArray<FColor> Pixels;
        FRenderTarget* Target = Picture ? Picture->GameThread_GetRenderTargetResource() : nullptr;
        if (!Test.TestTrue(Where + TEXT(": read the hole map"), Target && Target->ReadPixels(Pixels) && !Pixels.IsEmpty()))
            return false;
        const int32 Width = Picture->SizeX, Height = Picture->SizeY;
        // The middle tenth across, between the tee box and the green.
        double Red = 0, Green = 0, Blue = 0;
        int32 Count = 0;
        for (int32 Y = Height / 5; Y < Height * 4 / 5; Y += 4)
        {
            for (int32 X = Width * 9 / 20; X < Width * 11 / 20; X += 2)
            {
                const FColor& Color = Pixels[Y * Width + X];
                Red += Color.R;
                Green += Color.G;
                Blue += Color.B;
                ++Count;
            }
        }
        Red /= Count;
        Green /= Count;
        Blue /= Count;
        constexpr double Margin = 10;
        return Test.TestTrue(FString::Printf(TEXT("%s: the hole map shows turf along the hole (mean RGB %.0f, %.0f, %.0f)"), *Where,
            Red, Green, Blue), Green > Red + Margin && Green > Blue + Margin);
    }
} // namespace HoleMapContent

#endif
