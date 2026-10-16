#pragma once

#include "AppConfig.h"

enum class ControlWatchdogStage {
    UseTarget,
    ZeroVelocity,
    ExitControl
};

class ControlWatchdog {
public:
    // exitWhenTimedOut is used while guided control is not yet confirmed.
    // After confirmation, a quiet stream hovers at zero velocity instead of leaving the mode.
    static ControlWatchdogStage stageForElapsedMs(qint64 elapsedMs,
                                                   const AppConfig &config,
                                                   bool exitWhenTimedOut = true);
};
