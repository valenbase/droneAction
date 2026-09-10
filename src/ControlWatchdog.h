#pragma once

#include "AppConfig.h"

enum class ControlWatchdogStage {
    UseTarget,
    ZeroVelocity,
    ExitControl
};

class ControlWatchdog {
public:
    static ControlWatchdogStage stageForElapsedMs(qint64 elapsedMs, const AppConfig &config);
};
