// SPDX-License-Identifier: MIT
#include "Modules/ModuleManager.h"

#include "OpenGolfSettings.h"
#if WITH_EDITOR
namespace CourseDesigner { void Register(); }
#endif
class FOpenGolfModule : public FDefaultGameModuleImpl
{
#if WITH_EDITOR
    virtual void StartupModule() override
    {
        if (!IsRunningCommandlet() && GIsEditor) CourseDesigner::Register();
    }
#endif
    virtual void ShutdownModule() override { GolfSettings::ShutdownDiagnostics(); }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FOpenGolfModule, UOpenGolf, "UOpenGolf");
