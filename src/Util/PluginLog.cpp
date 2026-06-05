#include "PluginLog.hpp"

namespace
{
RED4ext::PluginHandle s_handle{};
const RED4ext::Sdk* s_sdk{};
} // namespace

void replay::log::Bind(RED4ext::PluginHandle aHandle, const RED4ext::Sdk* aSdk) noexcept
{
    s_handle = aHandle;
    s_sdk = aSdk;
}

void replay::log::Info(const char* aMessage) noexcept
{
    if (s_sdk && s_sdk->logger)
        s_sdk->logger->Info(s_handle, aMessage);
}

void replay::log::Warn(const char* aMessage) noexcept
{
    if (s_sdk && s_sdk->logger)
        s_sdk->logger->Warn(s_handle, aMessage);
}

void replay::log::Error(const char* aMessage) noexcept
{
    if (s_sdk && s_sdk->logger)
        s_sdk->logger->Error(s_handle, aMessage);
}
