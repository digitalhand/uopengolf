// SPDX-License-Identifier: MIT
#include "UOpenGolfHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"

FText AUOpenGolfHUD::GetGreeting() const
{
    return NSLOCTEXT("UOpenGolf", "HelloWorld", "Hello, Unreal Open Golf");
}

void AUOpenGolfHUD::DrawHUD()
{
    Super::DrawHUD();

    if (Canvas && GEngine)
    {
        const FString Greeting = GetGreeting().ToString();
        UFont* Font = GEngine->GetLargeFont();
        float Width = 0.0f;
        float Height = 0.0f;
        GetTextSize(Greeting, Width, Height, Font, 2.0f);
        DrawText(Greeting, FLinearColor::White,
            (Canvas->SizeX - Width) * 0.5f, (Canvas->SizeY - Height) * 0.5f,
            Font, 2.0f);
    }
}
