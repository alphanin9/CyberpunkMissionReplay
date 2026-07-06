#pragma once

// Hand-authored override of generated `quest/SignalStoppingNodeDefinition.hpp`.
//
// Abstract base of the *yielding* node family (questPauseConditionNodeDefinition,
// questCheckpointNodeDefinition, ...). Adds no new vtable slots or fields: the suspend hooks
// already exist on NodeDefinition (GetSignalName @0x180, BuildListener @0x188) and the
// register/wake plumbing lives in the engine (SignalStopping_RegisterPending 0x1402581e0 +
// QuestPhaseContext). A concrete yielding node overrides ExecuteNode (to return 1 / suspend)
// and the two suspend hooks. See QUEST_NODES_RESEARCH.md §4/§4A.

#include <RED4ext/Common.hpp>
#include <RED4ext/Scripting/Natives/questDisableableNodeDefinition.hpp>

namespace RED4ext
{
namespace quest
{
struct SignalStoppingNodeDefinition : quest::DisableableNodeDefinition
{
    static constexpr const char* NAME = "questSignalStoppingNodeDefinition";
    static constexpr const char* ALIAS = NAME;
};
RED4EXT_ASSERT_SIZE(SignalStoppingNodeDefinition, 0x48);
} // namespace quest

using questSignalStoppingNodeDefinition = quest::SignalStoppingNodeDefinition;
} // namespace RED4ext
