#pragma once

#include "features/desktop_pet/desktop_pet_mode.h"

// User visibility belongs to ReplicaConfig. These flags only affect effective
// presentation, so overlapping temporary modes can be lifted in any order.
class RuntimePolicy final {
public:
    DesktopPetMode desktopPetMode{DesktopPetMode::Resident};
    bool regionCapture{};

    [[nodiscard]] bool replicaVisible(bool requested) const noexcept
    {
        return requested && !presentationSuppressed() && !regionCapture;
    }
    [[nodiscard]] bool automaticActivation(bool requested) const noexcept
    {
        return requested && !presentationSuppressed() && !regionCapture;
    }
    [[nodiscard]] bool audioEnabled() const noexcept { return !presentationSuppressed(); }
    [[nodiscard]] bool presentationSuppressed() const noexcept
    {
        return desktopPetMode != DesktopPetMode::Resident;
    }
};
