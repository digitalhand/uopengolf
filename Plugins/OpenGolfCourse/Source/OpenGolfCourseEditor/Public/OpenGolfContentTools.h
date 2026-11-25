// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenGolfContentTools.generated.h"

/** Editor content upkeep for scripts (Docs/ContentNaming.md#renaming-content). */
UCLASS()
class OPENGOLFCOURSEEDITOR_API UOpenGolfContentTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /**
     * Points every referencer of the redirectors under Paths at their targets, saves the referencers and deletes the
     * redirectors that are no longer needed, as the Content Browser's "Update Redirector References" does.
     * Returns the number of redirectors found; FixedUp is how many were deleted.
     */
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Content")
    static int32 FixUpRedirectors(const TArray<FString>& Paths, int32& FixedUp);
};
