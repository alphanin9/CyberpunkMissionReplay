#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

using namespace Red;

namespace init {
	RED4EXT_C_EXPORT bool RED4EXT_CALL Main(PluginHandle aHandle, EMainReason aReason, const Sdk* aSdk) {
		switch (aReason) {
		case EMainReason::Load:
			TypeInfoRegistrar::RegisterDiscovered();
			break;
		}
		return true;
	}

	RED4EXT_C_EXPORT void RED4EXT_CALL Query(PluginInfo* aInfo) {
		aInfo->name = L"Mission Replay";
		aInfo->author = L"not_alphanine";
		aInfo->version = RED4EXT_SEMVER_EX(0, 0, 0, RED4EXT_V0_SEMVER_PRERELEASE_TYPE_ALPHA, 1);
		aInfo->runtime = RED4EXT_RUNTIME_INDEPENDENT;
		aInfo->sdk = RED4EXT_SDK_LATEST;
	}

	RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() {
		return RED4EXT_API_VERSION_LATEST;
	}
}