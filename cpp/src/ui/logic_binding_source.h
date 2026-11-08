#pragma once

#include "config/config_types.h"
#include "platform/native_gateway.h"

inline const WindowInfo* resolveLogicBindingSource(
    const LogicBindingConfig& binding, const QList<WindowInfo>& sources)
{
    if (binding.sourceTitle.isEmpty()) { return nullptr; }
    const WindowInfo* result = nullptr;
    for (const auto& source : sources) {
        if (!source.hwnd || source.title != binding.sourceTitle
            || source.processName.compare(binding.sourceProcessName, Qt::CaseInsensitive) != 0) { continue; }
        if (result) { return nullptr; }
        result = &source;
    }
    return result;
}
