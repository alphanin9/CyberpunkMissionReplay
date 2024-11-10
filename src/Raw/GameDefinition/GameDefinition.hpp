#pragma once
#include <Detail/AddressHashes.hpp>
#include <Util/Core.hpp>
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/gsm/GameDefinition.hpp>
#include <RED4ext/Scripting/Natives/worldWorldID.hpp>

using namespace Red;

namespace raw
{
namespace GameDefinition
{
	constexpr auto ToWorldID = Util::RawFunc<UsedAddressHashes::GameDefinition_ToWorldID, world::WorldID*(*)(gsm::GameDefinition*, world::WorldID*)>();
};
}; // namespace raw::gsm