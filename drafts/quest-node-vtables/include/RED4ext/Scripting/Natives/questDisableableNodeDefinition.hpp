#pragma once

// Hand-authored override of generated `quest/DisableableNodeDefinition.hpp`.
// Adds no new vtable slots or fields over questNodeDefinition; exists so the rest of the
// hierarchy (and custom nodes) inherit the vtable-bearing NodeDefinition declared above.

#include <RED4ext/Common.hpp>
#include <RED4ext/Scripting/Natives/questNodeDefinition.hpp>

namespace RED4ext
{
namespace quest
{
struct DisableableNodeDefinition : quest::NodeDefinition
{
    static constexpr const char* NAME = "questDisableableNodeDefinition";
    static constexpr const char* ALIAS = NAME;
};
RED4EXT_ASSERT_SIZE(DisableableNodeDefinition, 0x48);
} // namespace quest

using questDisableableNodeDefinition = quest::DisableableNodeDefinition;
} // namespace RED4ext
