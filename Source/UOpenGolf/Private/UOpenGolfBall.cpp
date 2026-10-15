// SPDX-License-Identifier: MIT
#include "UOpenGolfBall.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AUOpenGolfBall::AUOpenGolfBall()
{
    // Import normalizes the mesh to 100 cm diameter, matching the base actor's scale.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/UOpenGolf/Art/SM_GolfBall.SM_GolfBall"));
    if (Mesh.Succeeded())
    {
        BallMesh->SetStaticMesh(Mesh.Object);
    }
}
