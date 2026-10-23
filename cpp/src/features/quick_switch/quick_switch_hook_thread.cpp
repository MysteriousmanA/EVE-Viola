#include "features/quick_switch/quick_switch_service_impl.h"

#include <utility>

#ifdef Q_OS_WIN

void QuickSwitchService::Impl::signalReady(bool success, QString error)
{
    {
        const std::lock_guard lock(startMutex);
        startSucceeded = success;
        startError = std::move(error);
        startFinished = true;
    }
    startCondition.notify_one();
    if (!success) {
        postError(startError);
    }
}

LRESULT CALLBACK QuickSwitchService::Impl::keyboardHook(int code, WPARAM message, LPARAM parameter)
{
    if (code != HC_ACTION) {
        return CallNextHookEx(nullptr, code, message, parameter);
    }
    auto* self = activeImpl.load(std::memory_order_acquire);
    if (self == nullptr || parameter == 0) {
        return CallNextHookEx(nullptr, code, message, parameter);
    }

    return self->handleKeyboardEvent(message, parameter);
}

LRESULT CALLBACK QuickSwitchService::Impl::mouseHookCallback(int code, WPARAM message, LPARAM parameter)
{
    if (code != HC_ACTION) {
        return CallNextHookEx(nullptr, code, message, parameter);
    }
    auto* self = activeImpl.load(std::memory_order_acquire);
    if (self == nullptr || parameter == 0) {
        return CallNextHookEx(nullptr, code, message, parameter);
    }

    return self->handleMouseEvent(message, parameter);
}

void QuickSwitchService::Impl::run()
{
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    threadId.store(GetCurrentThreadId(), std::memory_order_release);

    Impl* expected = nullptr;
    if (!activeImpl.compare_exchange_strong(
            expected, this, std::memory_order_acq_rel)) {
        signalReady(false, QStringLiteral("快捷切屏钩子已经运行"));
        threadId.store(0, std::memory_order_release);
        return;
    }

    keyboardHookHandle = SetWindowsHookExW(
        WH_KEYBOARD_LL, &Impl::keyboardHook, GetModuleHandleW(nullptr), 0);
    if (keyboardHookHandle == nullptr) {
        activeImpl.store(nullptr, std::memory_order_release);
        const DWORD error = GetLastError();
        signalReady(
            false,
            QStringLiteral("安装键盘钩子失败，错误码 %1").arg(error));
        threadId.store(0, std::memory_order_release);
        return;
    }

    mouseHook = SetWindowsHookExW(
        WH_MOUSE_LL, &Impl::mouseHookCallback, GetModuleHandleW(nullptr), 0);
    if (mouseHook == nullptr) {
        const DWORD error = GetLastError();
        UnhookWindowsHookEx(keyboardHookHandle);
        keyboardHookHandle = nullptr;
        activeImpl.store(nullptr, std::memory_order_release);
        signalReady(
            false,
            QStringLiteral("安装鼠标钩子失败，错误码 %1").arg(error));
        threadId.store(0, std::memory_order_release);
        return;
    }

    signalReady(true);
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWindowsHookEx(mouseHook);
    mouseHook = nullptr;
    UnhookWindowsHookEx(keyboardHookHandle);
    keyboardHookHandle = nullptr;
    releaseTrackedMouseButtons();
    releaseTrackedKeyboardKeys();
    activeImpl.store(nullptr, std::memory_order_release);
    threadId.store(0, std::memory_order_release);
}

#endif

void QuickSwitchService::start(
    const QStringList& previousKeys,
    const QStringList& nextKeys,
    const QString& radialSwitchKey)
{
#ifdef Q_OS_WIN
    updateKeys(previousKeys, nextKeys);
    updateRadialSwitchKey(radialSwitchKey);
    if (impl_->thread.joinable()) {
        return;
    }

    {
        const std::lock_guard lock(impl_->startMutex);
        impl_->startFinished = false;
        impl_->startSucceeded = false;
        impl_->startError.clear();
    }
    impl_->thread = std::thread([impl = impl_.get()] {
        impl->run();
    });

    std::unique_lock lock(impl_->startMutex);
    impl_->startCondition.wait(lock, [this] {
        return impl_->startFinished;
    });
    const bool started = impl_->startSucceeded;
    lock.unlock();
    if (!started && impl_->thread.joinable()) {
        impl_->thread.join();
    }
#else
    Q_UNUSED(previousKeys);
    Q_UNUSED(nextKeys);
    Q_UNUSED(radialSwitchKey);
    emit hookError(QStringLiteral("快捷切屏仅支持 Windows"));
#endif
}

void QuickSwitchService::stop()
{
#ifdef Q_OS_WIN
    impl_->captureArmed.store(false, std::memory_order_release);
    if (!impl_->thread.joinable()) {
        return;
    }
    const DWORD threadId = impl_->threadId.load(std::memory_order_acquire);
    if (threadId != 0) {
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    impl_->thread.join();
#endif
}
