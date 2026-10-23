#pragma once

#include <QMetaType>
#include <QString>

enum class DesktopPetMode { Resident, Fish, DoNotDisturb };
Q_DECLARE_METATYPE(DesktopPetMode)

inline bool isValidDesktopPetMode(DesktopPetMode mode)
{
    return mode == DesktopPetMode::Resident || mode == DesktopPetMode::Fish
        || mode == DesktopPetMode::DoNotDisturb;
}

inline QString desktopPetModeName(DesktopPetMode mode)
{
    switch (mode) {
    case DesktopPetMode::Resident: return QStringLiteral("常驻模式");
    case DesktopPetMode::Fish: return QStringLiteral("摸鱼模式");
    case DesktopPetMode::DoNotDisturb: return QStringLiteral("勿扰模式");
    }
    return QStringLiteral("常驻模式");
}

inline QString desktopPetModeKey(DesktopPetMode mode)
{
    switch (mode) {
    case DesktopPetMode::Fish: return QStringLiteral("fish");
    case DesktopPetMode::DoNotDisturb: return QStringLiteral("doNotDisturb");
    default: return QStringLiteral("resident");
    }
}

inline DesktopPetMode desktopPetModeFromKey(const QString& key)
{
    if (key == QStringLiteral("fish")) { return DesktopPetMode::Fish; }
    if (key == QStringLiteral("doNotDisturb")) { return DesktopPetMode::DoNotDisturb; }
    return DesktopPetMode::Resident;
}
