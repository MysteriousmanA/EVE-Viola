#include "features/quick_switch/quick_switch_service.h"

#include "features/quick_switch/quick_switch_service_impl.h"

QuickSwitchService::QuickSwitchService(NativeGateway& gateway, QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>(this, gateway))
{
}

QuickSwitchService::~QuickSwitchService()
{
    stop();
}
