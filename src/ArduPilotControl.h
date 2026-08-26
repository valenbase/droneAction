#pragma once

#include "CommandTypes.h"

#include <QString>
#include <cstdint>

namespace ArduPilotControl {

constexpr std::uint32_t CopterGuidedMode = 4;
constexpr std::uint16_t VelocityAndYawRateTypeMask = 1479;

struct BodyVelocityTarget {
    float forwardMps = 0.0f;
    float rightMps = 0.0f;
    float downMps = 0.0f;
    float yawRateRadS = 0.0f;
};

BodyVelocityTarget bodyVelocityTarget(const DroneCommand &command);
QString copterModeDisplayName(std::uint32_t customMode);

}
