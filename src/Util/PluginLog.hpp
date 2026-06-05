#pragma once
#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <RED4ext/Api/v1/Sdk.hpp>

namespace replay::log
{
// Bind the RED4ext logger so messages also land in the red4ext log file.
// The CET-visible Red::Log::Channel side is always wired (no bind needed).
void Bind(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk) noexcept;

void Info(const char* aMessage) noexcept;
void Warn(const char* aMessage) noexcept;
void Error(const char* aMessage) noexcept;
} // namespace replay::log
