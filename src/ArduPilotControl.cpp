#include "ArduPilotControl.h"

#include <QtMath>

namespace ArduPilotControl {

BodyVelocityTarget bodyVelocityTarget(const DroneCommand &command)
{
    BodyVelocityTarget target;
    const float value = static_cast<float>(qAbs(command.value));

    switch (command.kind) {
    case CommandKind::Forward:
        target.forwardMps = value;
        break;
    case CommandKind::Backward:
        target.forwardMps = -value;
        break;
    case CommandKind::Right:
        target.rightMps = value;
        break;
    case CommandKind::Left:
        target.rightMps = -value;
        break;
    case CommandKind::Up:
        target.downMps = -value;
        break;
    case CommandKind::Down:
        target.downMps = value;
        break;
    case CommandKind::YawRight:
        target.yawRateRadS = qDegreesToRadians(value);
        break;
    case CommandKind::YawLeft:
        target.yawRateRadS = -qDegreesToRadians(value);
        break;
    default:
        break;
    }

    return target;
}

QString copterModeDisplayName(std::uint32_t customMode)
{
    switch (customMode) {
    case 0:
        return QStringLiteral("Stabilized");
    case 1:
        return QStringLiteral("Acro");
    case 2:
        return QStringLiteral("Altitude control");
    case 3:
        return QStringLiteral("Mission");
    case CopterGuidedMode:
        return QStringLiteral("Guided control");
    case 5:
        return QStringLiteral("Position hold");
    case 6:
        return QStringLiteral("Return to launch");
    case 7:
        return QStringLiteral("Circle");
    case 9:
        return QStringLiteral("Land");
    case 13:
        return QStringLiteral("Sport");
    case 16:
        return QStringLiteral("Position hold");
    case 17:
        return QStringLiteral("Brake");
    case 20:
        return QStringLiteral("Guided no GPS");
    case 21:
        return QStringLiteral("Smart return");
    case 23:
        return QStringLiteral("Follow me");
    default:
        return QStringLiteral("Mode %1").arg(customMode);
    }
}

}
