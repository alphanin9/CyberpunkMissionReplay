#pragma once

// Hand-authored override of the generated POD `quest/NodeDefinition.hpp`.
//
// The generated header is a layout mirror (fields only). This version additionally declares
// the *virtual table* of the quest node-definition hierarchy so that a C++ subclass gets a
// vtable matching the engine's, exactly like RED4ext declares `ISerializable` / `IGameSystem`.
//
// Slots 0x00..0xD0 (0..26) belong to `ISerializable` (inherited, unchanged). The 23 virtuals
// declared below (0xD8..0x188, slots 27..49) are the graph-node + quest-node block. Verified
// against `Cyberpunk2077.exe` `questNodeDefinition::vtbl @ 0x142af9c70` — see
// QUEST_NODES_RESEARCH.md §2/§9 for the dump and address-hash provenance.
//
// ExecuteNode is slot 43 (0x158). On the engine base it is pure (`purecall`); kept pure here
// so every concrete node must implement it. The standard socket pre-/post-pass and the
// signal-stopping hooks are given default *forwarders* (see -inl) so a leaf node only has to
// implement ExecuteNode (+ GetSignalName/BuildListener if it yields).

#include <cstdint>

#include <RED4ext/Common.hpp>
#include <RED4ext/CName.hpp>
#include <RED4ext/Handle.hpp>
#include <RED4ext/Containers/DynArray.hpp>
#include <RED4ext/Scripting/Natives/Generated/graph/GraphNodeDefinition.hpp>

namespace RED4ext
{
namespace quest
{
// The object every ExecuteNode receives as its context (engine vtable off_142AC3138).
// Not modelled here; treated opaquely. See QUEST_NODES_RESEARCH.md §4A.
struct QuestPhaseContext;

struct NodeDefinition : graph::GraphNodeDefinition
{
    static constexpr const char* NAME = "questNodeDefinition";
    static constexpr const char* ALIAS = NAME;

    // --- graph-node socket management (engine declares these on graphGraphNodeDefinition; ----
    // --- consolidated here because the generated graph PODs carry no virtuals) --------------
    virtual void RebuildSocketsAndResetOnFailure(); // D8  (27)  engine 0x14270de74
    virtual void sub_E0();                          // E0  (28)  no-op stub
    virtual void ResetSocketsAndNodeId();           // E8  (29)  engine 0x14270debc
    virtual void ClearCachedSocketHandles();        // F0  (30)  engine 0x140671a7c
    virtual void RebuildCachedSocketHandles();      // F8  (31)  engine 0x14270df74
    virtual void sub_100();                         // 100 (32)  no-op stub
    virtual void sub_108();                         // 108 (33)  no-op stub
    virtual void sub_110(void* a1);                 // 110 (34)  engine 0x140761918
    virtual void sub_118();                         // 118 (35)  no-op stub
    virtual void ClearHandleOut(void* aOut);        // 120 (36)  engine 0x1418e41f0
    virtual bool sub_128();                         // 128 (37)  return false stub
    virtual void sub_130();                         // 130 (38)  no-op stub
    virtual void sub_138();                         // 138 (39)  no-op stub
    virtual void sub_140();                         // 140 (40)  no-op stub
    virtual bool sub_148();                         // 148 (41)  return false stub

    // --- node execution (the hot path) ------------------------------------------------------
    // Pre-pass: socket-kind enumeration. Pure on the engine base; defaulted to the standard
    // concrete impl (DispatchSocketEnumerationByKind, 0x14058e78c) so leaves needn't reimplement.
    virtual uint8_t DispatchSocketEnumerationByKind(QuestPhaseContext* aCtx,
                                                    int64_t aInputSocket); // 150 (42)

    // The per-node behaviour. Return 0 = done (emit output, advance), 1 = yield (suspend),
    // 2 = embedded sub-phase, 3 = cut/interrupt. PURE: every node implements it.
    virtual uint8_t ExecuteNode(QuestPhaseContext* aCtx, int64_t aInputSocket,
                                DynArray<CName>& aOutputSockets) = 0; // 158 (43)  ** ExecuteNode **

    // Post-pass: maps the ExecuteNode return into the signal flow (ReturnInputBool, 0x14058fc34).
    virtual uint8_t ReturnInputBool(QuestPhaseContext* aCtx, uint8_t aExecuteResult,
                                    DynArray<CName>& aOutputSockets); // 160 (44)

    virtual void BuildGraphNodeDescriptor(void* aOut, void* a2);      // 168 (45)  engine 0x140592a34
    virtual void* GetGraphNodeVariant(void* aOut);                    // 170 (46)  engine 0x1418fc470
    virtual void* sub_178();                                          // 178 (47)  shared stub

    // --- signal-stopping hooks (used by SignalStopping_RegisterPending, 0x1402581e0) --------
    // Base defaults are no-op/return; the SignalStopping family overrides them. A non-yielding
    // node leaves them defaulted.
    virtual CName* GetSignalName(CName& aOut);                        // 180 (48)
    virtual void* BuildListener(void* aOut, QuestPhaseContext* aCtx); // 188 (49)  engine base 0x1424b1eb8

    uint16_t id;                  // 40
    uint8_t unk42[0x48 - 0x42];   // 42
};
RED4EXT_ASSERT_SIZE(NodeDefinition, 0x48);
RED4EXT_ASSERT_OFFSET(NodeDefinition, id, 0x40);
} // namespace quest

using questNodeDefinition = quest::NodeDefinition;
} // namespace RED4ext

#ifndef RED4EXT_STATIC_LIB
#include <RED4ext/Scripting/Natives/questNodeDefinition-inl.hpp>
#endif
