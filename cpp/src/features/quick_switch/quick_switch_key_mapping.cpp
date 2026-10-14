#include "features/quick_switch/quick_switch_service_impl.h"

#include "features/quick_switch/quick_switch_keys.h"

#ifdef Q_OS_WIN

QuickSwitchService::Impl::Impl(QuickSwitchService* service, NativeGateway& nativeGateway)
    : owner(service)
    , gateway(nativeGateway)
{
    for (auto& direction : keyboardDirections) {
        direction.store(kNoDirection, std::memory_order_relaxed);
    }
}

int QuickSwitchService::Impl::keyboardIndexForKey(const QString& key)
{
    const auto input = quickSwitchKeyboardInput(key);
    if (!input.has_value()) {
        return -1;
    }
    return input->virtualKey * 2 + (input->extended ? 1 : 0);
}

std::uint8_t QuickSwitchService::Impl::mouseMaskForKeys(
    const QStringList& previousKeys,
    const QStringList& nextKeys)
{
    std::uint8_t mask = 0;
    for (int token = kMouse4Token; token <= kMouse5Token; ++token) {
        const QString key = token == kMouse4Token
            ? QStringLiteral("Mouse4")
            : QStringLiteral("Mouse5");
        if (previousKeys.contains(key)) {
            mask = static_cast<std::uint8_t>(mask | (1u << token));
        }
        if (nextKeys.contains(key)) {
            mask = static_cast<std::uint8_t>(mask | (1u << (token + 4)));
        }
    }
    return mask;
}

void QuickSwitchService::Impl::setKeys(const QStringList& previousKeys, const QStringList& nextKeys)
{
    std::array<std::int8_t, kKeyboardIdentityCount> directions{};
    directions.fill(kNoDirection);
    for (const QString& key : previousKeys) {
        const int index = keyboardIndexForKey(key);
        if (index >= 0 && index < kKeyboardIdentityCount) {
            directions[static_cast<std::size_t>(index)] = kPreviousDirection;
        }
    }
    for (const QString& key : nextKeys) {
        const int index = keyboardIndexForKey(key);
        if (index >= 0 && index < kKeyboardIdentityCount
            && directions[static_cast<std::size_t>(index)] == kNoDirection) {
            directions[static_cast<std::size_t>(index)] = kNextDirection;
        }
    }
    switchMappingVersion.fetch_add(1, std::memory_order_seq_cst);
    for (std::size_t index = 0; index < directions.size(); ++index) {
        keyboardDirections[index].store(
            directions[index], std::memory_order_seq_cst);
    }
    switchKeys.store(
        mouseMaskForKeys(previousKeys, nextKeys), std::memory_order_seq_cst);
    switchMappingVersion.fetch_add(1, std::memory_order_seq_cst);
}

void QuickSwitchService::Impl::setRadialSwitchKey(const QString& key)
{
    int index = keyboardIndexForKey(key);
    if (index < 0 || index >= kKeyboardIdentityCount) {
        index = 0x20 * 2;
    }
    radialSwitchKeyIndex.store(index, std::memory_order_release);
}

int QuickSwitchService::Impl::mouseDirection(int token) const
{
    if (token < kMouse4Token || token > kMouse5Token) {
        return kNoDirection;
    }
    const std::uint32_t version = switchMappingVersion.load(
        std::memory_order_seq_cst);
    if ((version & 1u) != 0) {
        return kNoDirection;
    }
    const std::uint8_t keys = switchKeys.load(std::memory_order_seq_cst);
    if (version != switchMappingVersion.load(std::memory_order_seq_cst)) {
        return kNoDirection;
    }
    const std::uint8_t tokenMask = static_cast<std::uint8_t>(1u << token);
    if ((keys & tokenMask) != 0) {
        return kPreviousDirection;
    }
    if (((keys >> 4) & tokenMask) != 0) {
        return kNextDirection;
    }
    return kNoDirection;
}

int QuickSwitchService::Impl::keyboardDirection(std::size_t index) const
{
    const std::uint32_t version = switchMappingVersion.load(
        std::memory_order_seq_cst);
    if ((version & 1u) != 0) {
        return kNoDirection;
    }
    const int direction = keyboardDirections[index].load(std::memory_order_seq_cst);
    return version == switchMappingVersion.load(std::memory_order_seq_cst)
        ? direction
        : kNoDirection;
}

#endif

void QuickSwitchService::updateKeys(
    const QStringList& previousKeys,
    const QStringList& nextKeys)
{
#ifdef Q_OS_WIN
    impl_->setKeys(previousKeys, nextKeys);
#else
    Q_UNUSED(previousKeys);
    Q_UNUSED(nextKeys);
#endif
}

void QuickSwitchService::updateRadialSwitchKey(const QString& key)
{
#ifdef Q_OS_WIN
    impl_->setRadialSwitchKey(key);
#else
    Q_UNUSED(key);
#endif
}
