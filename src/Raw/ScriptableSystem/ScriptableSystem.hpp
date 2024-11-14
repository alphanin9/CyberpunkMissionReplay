#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <Detail/AddressHashes.hpp>
#include <Util/Core.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/ScriptableSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ScriptableSystemRequest.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ScriptableSystemsContainer.hpp>

using namespace Red;

namespace raw
{
namespace ScriptableSystemsContainer
{
constexpr auto GetSystemByName = util::RawFunc<UsedAddressHashes::ScriptableSystemsContainer_GetSystemByName,
                                               void* (*)(game::ScriptableSystemsContainer* aThis,
                                                         Handle<game::ScriptableSystem>& aSystem, CName aHash)>();
};

namespace ScriptableSystem
{
constexpr auto QueueRequest =
    util::RawFunc<UsedAddressHashes::ScriptableSystem_QueueRequest,
                  void* (*)(game::ScriptableSystem* aThis, Handle<game::ScriptableSystemRequest>& aRequest)>();
};
}; // namespace raw