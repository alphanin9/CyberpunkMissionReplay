#pragma once
#include <RED4ext/Api/v1/Sdk.hpp>
#include <RED4ext/Api/v1/PluginHandle.hpp>

namespace replay::log
{
void Bind(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk) noexcept;

void Info(const char* aMessage) noexcept;
void Warn(const char* aMessage) noexcept;
void Error(const char* aMessage) noexcept;
} // namespace replay::log
