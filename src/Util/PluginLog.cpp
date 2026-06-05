#include "PluginLog.hpp"

#include <RedLib.hpp>
#include <Shared/Util/NamePoolRegistrar.hpp>

namespace
{
RED4ext::v1::PluginHandle s_handle{};
const RED4ext::v1::Sdk* s_sdk{};

using ChannelName = shared::util::NamePoolRegistrar<"MissionReplay">;

void PostChannel(const char* aPrefix, const char* aMessage) noexcept
{
    // Surfacing in CET via Red::Log::Channel routes through RTTI's LogChannel,
    // which expects the name pool to know the channel CName.
    Red::Log::Channel(ChannelName::Get(), Red::CString(aPrefix) + Red::CString(aMessage));
}
} // namespace

void replay::log::Bind(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk) noexcept
{
    s_handle = aHandle;
    s_sdk = aSdk;
}

void replay::log::Info(const char* aMessage) noexcept
{
    if (s_sdk && s_sdk->logger)
        s_sdk->logger->Info(s_handle, aMessage);
    PostChannel("[info] ", aMessage);
}

void replay::log::Warn(const char* aMessage) noexcept
{
    if (s_sdk && s_sdk->logger)
        s_sdk->logger->Warn(s_handle, aMessage);
    PostChannel("[warn] ", aMessage);
}

void replay::log::Error(const char* aMessage) noexcept
{
    if (s_sdk && s_sdk->logger)
        s_sdk->logger->Error(s_handle, aMessage);
    PostChannel("[err] ", aMessage);
}
