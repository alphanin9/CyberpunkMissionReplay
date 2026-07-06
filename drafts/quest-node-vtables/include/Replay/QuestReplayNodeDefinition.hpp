#pragma once

// DRAFT: a custom *dynamic* quest node definition for replay comms.
//
//   questNodeDefinition -> questDisableableNodeDefinition -> questSignalStoppingNodeDefinition
//                       -> questReplayNodeDefinition   (this class)
//
// This is the corrected "dynamic quest node" shape (QUEST_NODES_RESEARCH.md): a custom node
// *definition* registered in RTTI, not a stock node carrying a custom embedded type. It carries
// typed fields authored in WolvenKit and read straight off `this` — no fact-name protocol.
//
// Two variants:
//   * Immediate (fire-and-forget): derive from questDisableableNodeDefinition instead and have
//     ExecuteNode do the work + return 0. No listener hooks needed.
//   * Yielding (wait-for-ack): the variant below — derive from questSignalStoppingNodeDefinition,
//     fire once then return 1 (suspend), and let native code wake it via QuestReplayNodeListener.
//
// Everything in the ExecuteNode body is reversed from questPauseConditionNodeDefinition
// (0x1404605e4) + questCheckpointNodeDefinition (0x14181d128). Exact QuestPhaseContext slot
// numbers are documented in QUEST_NODES_RESEARCH.md §4A and treated as [verify in-game].

#include <cstdint>

#include <RED4ext/Common.hpp>
#include <RED4ext/CName.hpp>
#include <RED4ext/NativeTypes.hpp>           // Variant
#include <RED4ext/Containers/DynArray.hpp>
#include <RED4ext/Scripting/Natives/questSignalStoppingNodeDefinition.hpp>

namespace replay
{
// Thin, offset-documented view over the engine QuestPhaseContext (vtable off_142AC3138).
// Only the slots a yielding node needs. See QUEST_NODES_RESEARCH.md §4A.
struct QuestPhaseContextView
{
    void** vtbl;

    bool IsNodePending(uint32_t aNodeId)            // slot 16 / 0x80
    {
        return reinterpret_cast<bool (*)(void*, uint32_t)>(vtbl[16])(this, aNodeId);
    }
    void UnregisterPendingNode(uint32_t aNodeId)    // slot 13 / 0x68
    {
        reinterpret_cast<void (*)(void*, uint32_t)>(vtbl[13])(this, aNodeId);
    }
};

struct ReplayNodeDefinition : RED4ext::quest::SignalStoppingNodeDefinition
{
    static constexpr const char* NAME = "questReplayNodeDefinition";
    static constexpr const char* ALIAS = "ReplayNodeDefinition";

    // --- authored payload (offsets after the 0x48 base; tune to taste, then assert) ---------
    RED4ext::CName command;    // 48  e.g. "INITIALIZE_ALL", "FINISHED"
    RED4ext::Variant payload;  // 50  optional typed argument
    uint32_t token;            // 60  per-node wake id (one node instance <-> one Resume)
    uint8_t unk64[0x68 - 0x64];// 64

    // --- overrides ---------------------------------------------------------------------------
    // The suspend/resume decision. Fire the command on first visit, yield (1) until native code
    // acks, then emit output and advance (0).
    uint8_t ExecuteNode(RED4ext::quest::QuestPhaseContext* aCtx, int64_t aInputSocket,
                        RED4ext::DynArray<RED4ext::CName>& aOutputSockets) override;

    // Suspend hooks the inherited SignalStopping_RegisterPending (0x1402581e0) calls.
    RED4ext::CName* GetSignalName(RED4ext::CName& aOut) override;                 // 180
    void* BuildListener(void* aOut, RED4ext::quest::QuestPhaseContext* aCtx) override; // 188
};
RED4EXT_ASSERT_OFFSET(ReplayNodeDefinition, command, 0x48);
RED4EXT_ASSERT_OFFSET(ReplayNodeDefinition, token, 0x60);
} // namespace replay
