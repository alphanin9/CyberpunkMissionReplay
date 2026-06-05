#pragma once
#include <RED4ext/Api/Sdk.hpp>

namespace replay::log
{
void Bind(RED4ext::PluginHandle aHandle, const RED4ext::Sdk* aSdk) noexcept;

void Info(const char* aMessage) noexcept;
void Warn(const char* aMessage) noexcept;
void Error(const char* aMessage) noexcept;
} // namespace replay::log
