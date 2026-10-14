#pragma once

// User visibility belongs to ReplicaConfig. These flags only affect effective
// presentation, so overlapping temporary modes can be lifted in any order.
class RuntimePolicy final {
public:
    bool fishMode{};
    bool regionCapture{};

    [[nodiscard]] bool replicaVisible(bool requested) const noexcept
    {
        return requested && !fishMode && !regionCapture;
    }
    [[nodiscard]] bool automaticActivation(bool requested) const noexcept
    {
        return requested && !fishMode && !regionCapture;
    }
    [[nodiscard]] bool audioEnabled() const noexcept { return !fishMode; }
};
