#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <Config/ProjectTemplate.hpp>
#include <Comms/ReplayComms.hpp>
#include <Util/PluginLog.hpp>

using namespace Red;

namespace init {
	RED4EXT_C_EXPORT bool RED4EXT_CALL Main(v1::PluginHandle aHandle, v1::EMainReason aReason, const v1::Sdk* aSdk) {
		switch (aReason) {
		case v1::EMainReason::Load:
			replay::log::Bind(aHandle, aSdk);
			TypeInfoRegistrar::RegisterDiscovered();
			break;
		}
		return true;
	}

	RED4EXT_C_EXPORT void RED4EXT_CALL Query(v1::PluginInfo* aInfo) {
		aInfo->name = L"Mission Replay";
		aInfo->author = L"not_alphanine";
		constexpr auto ModVersion = Build::GetModVersion();
		aInfo->version = RED4EXT_V1_SEMVER(
			static_cast<std::uint8_t>(ModVersion.major()),
			static_cast<std::uint8_t>(ModVersion.minor()),
			static_cast<std::uint8_t>(ModVersion.patch()));
		aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_INDEPENDENT;
		aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
	}

	RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() {
		return RED4EXT_API_VERSION_1;
	}
}
