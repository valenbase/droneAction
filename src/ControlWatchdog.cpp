#include "ControlWatchdog.h"

ControlWatchdogStage ControlWatchdog::stageForElapsedMs(qint64 elapsedMs, const AppConfig &config)
{
    if (elapsedMs >= config.controlExitTimeoutMs) {
        return ControlWatchdogStage::ExitControl;
    }
    if (elapsedMs >= config.commandStaleTimeoutMs) {
        return ControlWatchdogStage::ZeroVelocity;
    }
    return ControlWatchdogStage::UseTarget;
}
