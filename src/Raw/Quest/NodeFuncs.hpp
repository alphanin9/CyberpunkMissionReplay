#pragma once
#include <Detail/AddressHashes.hpp>
#include <Util/Core.hpp>

#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/quest/NodeDefinition.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/SetSaveDataLoadingScreen_NodeType.hpp>

using namespace Red;

namespace raw::Quest
{
namespace SetNextLoadingScreen
{
constexpr auto Execute = Util::RawFunc<UsedAddressHashes::Quest_SetSaveDataLoadingScreen_NodeType_ExecuteNode,
                                       char (*)(quest::SetSaveDataLoadingScreen_NodeType* aThis)>();
}
namespace FactsDBManager
{
using ExecuteType = char (*)(quest::NodeDefinition* aThis, void* aCtx, int64_t a3, DynArray<CName>& aOutSockets);
constexpr auto Execute =
    Util::RawFunc<UsedAddressHashes::Quest_FactsDBManagerNodeDefinition_ExecuteNode, ExecuteType>();
} // namespace FactsDBManager
} // namespace raw::Quest