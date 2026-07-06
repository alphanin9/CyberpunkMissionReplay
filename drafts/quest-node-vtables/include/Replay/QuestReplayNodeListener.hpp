#pragma once

// DRAFT: the wake side of a custom yielding quest node (QuestReplayNodeDefinition).
//
// A signal-stopping node, while suspended, holds a *listener descriptor* registered in the
// active QuestPhaseInstance pending-node map (phaseInstance+152). When its event source fires,
// the descriptor re-injects a quest signal that resumes the node. We reuse the engine's
// SetState/Refire verbatim (they only touch descriptor fields + owner->vtbl[0x08]); the only
// new code is Activate (subscribe to OUR wake source) and Resume (set the flag).
//
// Descriptor field offsets and the SetState/Refire addresses are reversed from the FactsDB
// listener (QUEST_NODES_RESEARCH.md §4A) and are [verify in-game].

#include <cstdint>
#include <unordered_map>

#include <RED4ext/Relocation.hpp>

namespace replay
{
// Engine listener primitives we reuse as-is (address hashes from QUEST_NODES_RESEARCH.md §9).
namespace detail
{
constexpr uint32_t Listener_SetState = 4263842581u; // 0x14045ec28  SetState(desc, satisfied) -> edge -> Refire
constexpr uint32_t Listener_Refire   = 776148020u;  // 0x14045fa44  owner(+0x18)->vtbl[0x08](owner, index@+0x28)
} // namespace detail

// Mirror of the engine descriptor (~88 bytes). Only the fields we touch are named.
struct ReplayNodeListener
{
    void** vtbl;            // 00  Activate @0x10, OnFired @0x60
    uint8_t unk08[0x18 - 0x08];
    void* owner;           // 18  re-fire target (phase signal owner); owner->vtbl[0x08] resumes
    void* parent;          // 20  logical nesting (null for us)
    uint8_t unk28[0x2C - 0x28];
    uint32_t index;        // 28  listener index passed back to owner on re-fire
    uint8_t unk2C[0x44 - 0x2C];
    int32_t satisfied;     // 44  cached flag read by SetState's edge detector
    uint8_t unk48[0x50 - 0x48];
    uint32_t token;        // 50  (engine: watched fact hash) -> our wake token
    uint8_t unk54[0x58 - 0x54];

    void SetState(bool aSatisfied)
    {
        using func_t = void (*)(ReplayNodeListener*, uint8_t);
        static RED4ext::UniversalRelocFunc<func_t> func(detail::Listener_SetState);
        func(this, aSatisfied ? 1 : 0);
    }
};

// Plain plugin singleton (NOT RTTI). Native code calls Resume(token) when replay setup acks.
class ReplayWakeRegistry
{
public:
    static ReplayWakeRegistry& Get()
    {
        static ReplayWakeRegistry s_instance;
        return s_instance;
    }

    void Register(uint32_t aToken, ReplayNodeListener* aListener) { m_listeners[aToken] = aListener; }
    void Unregister(uint32_t aToken) { m_listeners.erase(aToken); }

    // Called from the ReplayManager request-drain on the quest/tick thread.
    void Resume(uint32_t aToken)
    {
        auto it = m_listeners.find(aToken);
        if (it != m_listeners.end())
        {
            m_acked.insert(aToken);
            it->second->SetState(true); // -> edge -> Refire -> re-inject signal -> re-run ExecuteNode
        }
        else
        {
            m_acked.insert(aToken);     // ack arrived before suspend; ExecuteNode will see it
        }
    }

    bool IsResumed(uint32_t aToken) const { return m_acked.count(aToken) != 0; }

private:
    std::unordered_map<uint32_t, ReplayNodeListener*> m_listeners;
    std::unordered_map<uint32_t, char> m_acked; // set<uint32_t> spelled portably
};
} // namespace replay

// ---------------------------------------------------------------------------------------------
// Illustrative ExecuteNode body for QuestReplayNodeDefinition (yielding variant). Pseudocode-y:
// EmitOutputSocket / FireNativeCommand / SignalStopping_RegisterPending are bound elsewhere via
// the hashes in QUEST_NODES_RESEARCH.md §9. Mirrors questPauseConditionNodeDefinition's shape.
//
//   uint8_t replay::ReplayNodeDefinition::ExecuteNode(QuestPhaseContext* aCtx, int64_t, DynArray<CName>& out)
//   {
//       auto* ctx = reinterpret_cast<replay::QuestPhaseContextView*>(aCtx);
//       if (ReplayWakeRegistry::Get().IsResumed(token))      // native side acked?
//       {
//           if (ctx->IsNodePending(id))
//               ctx->UnregisterPendingNode(id);              // tears down the listener
//           EmitOutputSocket(out);                            // sub_1401943F4 (hash 3919059478)
//           return 0;                                         // advance -> signal step forwards
//       }
//       if (!ctx->IsNodePending(id))                          // first visit
//       {
//           FireNativeCommand(command, payload);              // enqueue a ReplayManager request, once
//           SignalStopping_RegisterPending(this, aCtx);       // 0x1402581e0: BuildListener+GetSignalName+Activate
//       }
//       return 1;                                             // yield; signal dies here
//   }
//
//   // BuildListener (slot 0x188): allocate a ReplayNodeListener, stamp token, return it.
//   // GetSignalName (slot 0x180): return the CName the listener is registered under.
//   // ReplayNodeListener::Activate (slot 0x10): ReplayWakeRegistry::Get().Register(token, this).
// ---------------------------------------------------------------------------------------------
