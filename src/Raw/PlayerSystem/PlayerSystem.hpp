#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/cp/PlayerSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/Object.hpp>

#include <Detail/AddressHashes.hpp>
#include <Util/Core.hpp>

using namespace Red;

namespace raw::PlayerSystem
{
constexpr auto GetPlayerControlledGameObject =
    util::RawFunc<UsedAddressHashes::PlayerSystem_GetPlayerControlledGameObject,
                  void* (*)(cp::PlayerSystem*, Handle<game::Object>&)>();
};