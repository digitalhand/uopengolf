// SPDX-License-Identifier: MIT
#include "Modules/ModuleManager.h"

#include "OpenGolfSettings.h"
class FOpenGolfModule : public FDefaultGameModuleImpl
{
    virtual void ShutdownModule() override { GolfSettings::ShutdownDiagnostics(); }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FOpenGolfModule, UOpenGolf, "UOpenGolf");
