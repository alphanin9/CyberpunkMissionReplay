#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/Scripting/Natives/questNodeDefinition.hpp>
#endif

#include <RED4ext/Relocation.hpp>

// Address hashes for the engine implementations of the node-definition virtuals.
// Each was recomputed from the function RVA in `Cyberpunk2077.exe` via the cp2077-address-hash
// skill and matched `cyberpunk2077_addresses.json` (same build as the research IDB).
// When upstreaming, move these into RED4ext/Detail/AddressHashes.hpp.
namespace RED4ext::Detail::QuestNodeHashes
{
constexpr uint32_t RebuildSocketsAndResetOnFailure = 1005067236u; // 0x14270de74
constexpr uint32_t ResetSocketsAndNodeId           = 3838515912u; // 0x14270debc
constexpr uint32_t ClearCachedSocketHandles        = 1930432548u; // 0x140671a7c
constexpr uint32_t RebuildCachedSocketHandles      = 3219460587u; // 0x14270df74
constexpr uint32_t sub_110                         = 3813347894u; // 0x140761918
constexpr uint32_t ClearHandleOut                  = 605168557u;  // 0x1418e41f0
constexpr uint32_t DispatchSocketEnumerationByKind = 2611489788u; // 0x14058e78c (concrete pre-pass)
constexpr uint32_t ReturnInputBool                 = 2762615183u; // 0x14058fc34
constexpr uint32_t BuildGraphNodeDescriptor        = 1529033043u; // 0x140592a34
constexpr uint32_t GetGraphNodeVariant             = 2456823096u; // 0x1418fc470
constexpr uint32_t BuildListenerBase               = 3665044997u; // 0x1424b1eb8
} // namespace RED4ext::Detail::QuestNodeHashes

namespace RNH = RED4ext::Detail::QuestNodeHashes;

// --- forwarders into the engine implementation -------------------------------------------
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::RebuildSocketsAndResetOnFailure()
{
    using func_t = void (*)(NodeDefinition*);
    static UniversalRelocFunc<func_t> func(RNH::RebuildSocketsAndResetOnFailure);
    func(this);
}

RED4EXT_INLINE void RED4ext::quest::NodeDefinition::ResetSocketsAndNodeId()
{
    using func_t = void (*)(NodeDefinition*);
    static UniversalRelocFunc<func_t> func(RNH::ResetSocketsAndNodeId);
    func(this);
}

RED4EXT_INLINE void RED4ext::quest::NodeDefinition::ClearCachedSocketHandles()
{
    using func_t = void (*)(NodeDefinition*);
    static UniversalRelocFunc<func_t> func(RNH::ClearCachedSocketHandles);
    func(this);
}

RED4EXT_INLINE void RED4ext::quest::NodeDefinition::RebuildCachedSocketHandles()
{
    using func_t = void (*)(NodeDefinition*);
    static UniversalRelocFunc<func_t> func(RNH::RebuildCachedSocketHandles);
    func(this);
}

RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_110(void* a1)
{
    using func_t = void (*)(NodeDefinition*, void*);
    static UniversalRelocFunc<func_t> func(RNH::sub_110);
    func(this, a1);
}

RED4EXT_INLINE void RED4ext::quest::NodeDefinition::ClearHandleOut(void* aOut)
{
    using func_t = void (*)(NodeDefinition*, void*);
    static UniversalRelocFunc<func_t> func(RNH::ClearHandleOut);
    func(this, aOut);
}

RED4EXT_INLINE uint8_t RED4ext::quest::NodeDefinition::DispatchSocketEnumerationByKind(
    QuestPhaseContext* aCtx, int64_t aInputSocket)
{
    using func_t = uint8_t (*)(NodeDefinition*, QuestPhaseContext*, int64_t);
    static UniversalRelocFunc<func_t> func(RNH::DispatchSocketEnumerationByKind);
    return func(this, aCtx, aInputSocket);
}

RED4EXT_INLINE uint8_t RED4ext::quest::NodeDefinition::ReturnInputBool(QuestPhaseContext* aCtx,
                                                                       uint8_t aExecuteResult,
                                                                       DynArray<CName>& aOutputSockets)
{
    using func_t = uint8_t (*)(NodeDefinition*, QuestPhaseContext*, uint8_t, DynArray<CName>&);
    static UniversalRelocFunc<func_t> func(RNH::ReturnInputBool);
    return func(this, aCtx, aExecuteResult, aOutputSockets);
}

RED4EXT_INLINE void RED4ext::quest::NodeDefinition::BuildGraphNodeDescriptor(void* aOut, void* a2)
{
    using func_t = void (*)(NodeDefinition*, void*, void*);
    static UniversalRelocFunc<func_t> func(RNH::BuildGraphNodeDescriptor);
    func(this, aOut, a2);
}

RED4EXT_INLINE void* RED4ext::quest::NodeDefinition::GetGraphNodeVariant(void* aOut)
{
    using func_t = void* (*)(NodeDefinition*, void*);
    static UniversalRelocFunc<func_t> func(RNH::GetGraphNodeVariant);
    return func(this, aOut);
}

RED4EXT_INLINE void* RED4ext::quest::NodeDefinition::BuildListener(void* aOut, QuestPhaseContext* aCtx)
{
    using func_t = void* (*)(NodeDefinition*, void*, QuestPhaseContext*);
    static UniversalRelocFunc<func_t> func(RNH::BuildListenerBase);
    return func(this, aOut, aCtx);
}

// --- trivial defaults (shared engine stubs: no-op / return false / pass-through) ----------
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_E0() {}
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_100() {}
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_108() {}
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_118() {}
RED4EXT_INLINE bool RED4ext::quest::NodeDefinition::sub_128() { return false; }
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_130() {}
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_138() {}
RED4EXT_INLINE void RED4ext::quest::NodeDefinition::sub_140() {}
RED4EXT_INLINE bool RED4ext::quest::NodeDefinition::sub_148() { return false; }
// Engine base reuses GetType() as the filler here (returns the class RTTI), so mirror that.
RED4EXT_INLINE void* RED4ext::quest::NodeDefinition::sub_178() { return GetType(); }
// Base slot 0x180 is also a GetType filler; the SignalStopping family overrides it with the
// real GetSignalName (delegates to the condition). Default keeps the (node, out)->out shape.
RED4EXT_INLINE RED4ext::CName* RED4ext::quest::NodeDefinition::GetSignalName(CName& aOut) { return &aOut; }
