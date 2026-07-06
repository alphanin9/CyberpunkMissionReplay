# Draft: quest node-definition vtable headers (RED4ext.SDK candidates)

Hand-authored headers that give the quest **node-definition** hierarchy a *proper C++ vtable*,
so a plugin can subclass `questSignalStoppingNodeDefinition` (or any `questNodeDefinition`) and
have the engine dispatch its `ExecuteNode`. Built in the style of RED4ext.SDK's hand-authored
`ISerializable` / `IGameSystem` (declare virtuals with `// offset` comments; define them in a
`-inl.hpp` as trivial defaults or `UniversalRelocFunc` address-hash forwarders).

Companion to `QUEST_NODES_RESEARCH.md` — read §2 (vtable layout), §4/§4A (suspend/resume), §6
(what "dynamic node" means), §9 (address-hash table). Every slot and hash here is verified
against `Cyberpunk2077.exe` (`questNodeDefinition::vtbl @ 0x142af9c70`); hashes recomputed with
the `cp2077-address-hash` skill.

## Files

| File | Purpose |
|---|---|
| `include/RED4ext/Scripting/Natives/questNodeDefinition.hpp` | Override of the generated POD; declares vtable slots 27–49 (`ExecuteNode` @ slot 43 / `0x158`) + `id`. |
| `…/questNodeDefinition-inl.hpp` | Default bodies + engine forwarders for the inherited virtuals. |
| `…/questDisableableNodeDefinition.hpp` | Chain link (no new slots/fields). |
| `…/questSignalStoppingNodeDefinition.hpp` | Abstract yielding base (no new slots/fields). |
| `include/Replay/QuestReplayNodeDefinition.hpp` | **Example** custom node: `questReplayNodeDefinition : questSignalStoppingNodeDefinition` with typed `command`/`payload`/`token`. |
| `include/Replay/QuestReplayNodeListener.hpp` | **Example** wake side: listener descriptor mirror + `ReplayWakeRegistry` + `ExecuteNode` body sketch. |

## How the vtable lines up

- Slots **0x00–0xD0 (0..26)** are `ISerializable` — inherited unchanged from the SDK. Our base
  vtable dump matched the SDK's `ISerializable` virtual list exactly (27 virtuals).
- Slots **0xD8–0x188 (27..49)** are the graph-node + quest-node block, declared in
  `questNodeDefinition.hpp`. Because the generated `graph::GraphNodeDefinition` /
  `graph::IGraphObjectDefinition` PODs carry **no** virtuals, declaring the whole block on
  `quest::NodeDefinition` puts `ExecuteNode` at the correct absolute slot 43 (`0x158`).
  - *Upstream note:* slots 27–46 are really graph-generic (socket management, descriptor,
    variant). When PR-ing to RED4ext.SDK they could migrate up into
    `graphGraphNodeDefinition.hpp`; the cumulative layout is what matters and is preserved
    either way. Left consolidated here so the draft is self-contained.
- `ExecuteNode` is kept **pure** (`= 0`) — every concrete node implements it, matching the
  engine (`purecall` on the base). The pre-/post-pass and signal-stopping hooks get default
  forwarders so a leaf only implements `ExecuteNode` (+ `GetSignalName`/`BuildListener` if it
  yields).

## Why a C++-`override` subclass is viable (not just a hand-built vtable)

RED4ext.SDK's `-inl.hpp` files **define** every declared virtual — as a trivial default or a
`UniversalRelocFunc` into the engine. So a subclass that overrides only a few slots links the
rest to those definitions, and the compiler-generated vtable is complete and valid. We do the
same for slots 27–49. This is the same mechanism that lets `ReplayManager : IGameSystem` work.

## Merging into RED4ext.SDK (per its CONTRIBUTING.md)

These follow the SDK's "declare reverse-engineered content" override procedure:
1. Drop `questNodeDefinition.hpp` etc. into `include/RED4ext/Scripting/Natives/` (the generated
   `Generated/quest/*.hpp` then get auto-disabled stubs by the RTTIDumper run).
2. Move the `Detail::QuestNodeHashes` constants into `RED4ext/Detail/AddressHashes.hpp`.
3. Keep the `RED4EXT_ASSERT_SIZE` / `RED4EXT_ASSERT_OFFSET` asserts; run clang-format.
4. Refine the `[verify]` signatures (socket/listener slots) against the RTTI dump.

## Testing in-tree first (before upstreaming)

`deps/red4ext.sdk` is a vendored submodule — don't edit it here. To compile these against it
without a name clash with the generated `RED4ext::quest::NodeDefinition`, either temporarily
shadow the include path or rename the draft namespace while testing. The custom node’s RTTI
parent is resolved by **registered class name** (`questSignalStoppingNodeDefinition`), not by
C++ type, so the C++ namespace doesn’t affect engine dispatch.

## The open piece — `[verify in-game]`

The headers give the correct **layout + vtable**. The remaining unknown is **construction**:
getting the quest-resource loader to instantiate `questReplayNodeDefinition` by class name with
*our* vtable. That needs a **native** RTTI registration (so the loader's `ConstructCls` builds
our C++ object), per the `psiberx/cp2077-codeware` / `archive-xl` precedent — a script/
`IScriptable`-backed registration would attach the wrong vtable (see QUEST_NODES_RESEARCH.md §6).
Validate that, plus the `[verify]` signatures and the listener field offsets, in-game before
relying on this.
