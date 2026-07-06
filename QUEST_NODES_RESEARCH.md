# Quest Nodes & Delayed (Yielding) Nodes — Reverse-Engineering Findings

> Research for issue #1 (replay comms via a **custom dynamic quest node** vs. the
> `FactsDBManager` hook).
> Source: `Cyberpunk2077.exe.i64` + `cyberpunk2077_addresses.json` (skill-cache copy),
> cross-checked against `deps/red4ext.sdk` generated headers and
> `deps/sharedpunk/.../Raw/Quest/*`.
> All function addresses are for the IDB's build; the **address-hash** column is the
> update-agnostic key (works with `RED4ext::UniversalRelocFunc` / `shared::util::RawFunc`).
> The IDB and the address list are the same build — every hash in §9 was recomputed from
> the function RVA via the `cp2077-address-hash` skill and matched the address list.

> **CORRECTION (this revision).** A "dynamic quest node" is **a custom node *definition*
> class** — a class you register that derives from `questNodeDefinition` (e.g.
> `questNodeDefinition → questSignalStoppingNodeDefinition → questReplayNodeDefinition`).
> It is **not** "a stock manager node carrying a custom embedded *type* object," which is
> how a previous revision of this document framed it. The engine instantiates a node by its
> registered class name straight off the quest resource and dispatches execution through
> that class's **own C++ object vtable** (`ExecuteNode`, slot 43). A custom node definition
> is therefore exactly the same kind of object as the ~80 stock `quest*NodeDefinition`
> classes — including the signal-stopping (yielding) ones — and gets the suspend/resume
> machinery by **inheriting `questSignalStoppingNodeDefinition`**, not by reimplementing it.

---

## 0. TL;DR

- **A quest node *is* a class deriving from `questNodeDefinition`.** Its per-visit logic is
  one virtual: **`ExecuteNode`, object-vtable slot 43 (offset `0x158`)**. On
  `questNodeDefinition` itself this slot is **`purecall`** (pure virtual) — every node
  class **must** override it. There are ~80 stock node-definition classes in the build, each
  with its own vtable and its own slot-43 `ExecuteNode`. A custom (plugin) node is one more
  of these.
- **`ExecuteNode`'s return value is the immediate-vs-yield switch:**
  - return **`0`** → node is *done this visit*: it emits its output socket(s); the signal
    step forwards the signal along those sockets to the next node(s) and the graph advances.
  - return **`1`** → node *yields*: the signal **dies at this node**; the node registers
    itself as a pending listener and is only re-`ExecuteNode`'d when an external event
    re-injects a signal into it.
  - return **`2`** → embedded sub-phase; **`3`** → "interrupted/cut" teardown path.
- **Yielding is a property of the node *class*, supplied by `questSignalStoppingNodeDefinition`.**
  `questPauseConditionNodeDefinition` and `questCheckpointNodeDefinition` derive from it;
  each provides its own `ExecuteNode` that decides when to suspend (return 1) and, while
  suspended, builds a listener (slots 48/49) and registers it with the phase. The
  register/unregister/wake plumbing lives in the `SignalStopping` base + the
  `QuestPhaseContext` and is **inherited** — a custom signal-stopping node reuses it.
- **The shared hook the replay code uses today is hit by every manager node.** Today the
  plugin hooks `FactsDBManager::Execute` (`0x1404321e8`, hash `3679006322u`). That function
  is the slot-43 `ExecuteNode` **shared** by `questFactsDBManagerNodeDefinition`,
  `questDynamicSpawnSystemNodeDefinition` and `questRewardManagerNodeDefinition` (verified
  at the vtable level), which is exactly the collision the issue flags.
- **Two ways to retire that hook, both built on a custom node definition:**
  1. **Immediate command node** — `questReplayNodeDefinition : questNodeDefinition`
     (or `: questDisableableNodeDefinition`). Override `ExecuteNode` to read a typed
     `CName command` / `Variant payload` straight off `this`, fire the native command, emit
     output, return 0. Fire-and-forget; cannot wait.
  2. **Yielding wait/ack node** — `questReplayNodeDefinition : questSignalStoppingNodeDefinition`.
     Override `ExecuteNode` (fire once, then `return 1` to suspend) and the listener hooks
     (slots 48/49) so the native side can wake it. Reuses the inherited
     `SignalStopping_RegisterPending` + `QuestPhaseContext` register/wake path.
- **The only genuinely hard part is the C++ vtable.** RED4ext's generated SDK headers are
  *layout mirrors* (POD structs with `// offset` comments), not virtual-method
  declarations, so a C++ subclass does not automatically get a vtable matching the engine's
  60-slot node-definition layout. You must reproduce that vtable (copy the stock base
  vtable, overwrite the few slots you implement) and register the class as a **native** type
  so the resource loader constructs it with *your* vtable. `psiberx/cp2077-codeware` and
  `archive-xl` are the precedent. This is **one** node-definition vtable derived from
  `SignalStopping`, **not** three hand-built vtables, and needs **no** embedded type/condition
  object. (The "IScriptable vtable" hazard from the prior revision applies only to
  *script*-registered classes; a native registration is dispatched through its real C++
  vtable — see §6.)

---

## 1. Class hierarchies (from generated SDK headers, RTTI-verified)

### Node definitions (the graph node objects)
`deps/red4ext.sdk/include/RED4ext/Scripting/Natives/Generated/...`:
```
graph::IGraphObjectDefinition
└─ graph::GraphNodeDefinition                 graph/GraphNodeDefinition.hpp
   └─ quest::NodeDefinition                   "questNodeDefinition"     size 0x48, id @0x40 (u16)
      └─ quest::DisableableNodeDefinition      "questDisableableNodeDefinition"   (abstract)
         ├─ quest::ConditionNodeDefinition        +Handle<IBaseCondition> condition @0x48   (branch)
         ├─ quest::FactsDBManagerNodeDefinition   +Handle<IFactsDBManagerNodeType> type @0x48   (IMMEDIATE)
         │   …and DynamicSpawnSystem / RewardManager / EventManager / … (all "manager+type", IMMEDIATE)
         └─ quest::SignalStoppingNodeDefinition   "questSignalStoppingNodeDefinition"  (abstract, YIELD-capable family)
            ├─ quest::PauseConditionNodeDefinition  +Handle<IBaseCondition> condition @0x48   (YIELDS)
            └─ quest::CheckpointNodeDefinition                                                  (YIELDS, own listener)
```
- `quest::NodeDefinition`, `quest::DisableableNodeDefinition`, `quest::SignalStoppingNodeDefinition`
  are **abstract**: their slot-43 `ExecuteNode` is `purecall` (`0x14190706c`). Concrete leaf
  classes override it.
- Manager nodes store a `Handle<…NodeType> type` at **+0x48**; condition/pause nodes store a
  `Handle<IBaseCondition> condition` at **+0x48**. The custom node defines **its own** typed
  fields after +0x48 — it does not need either of these handles.
- RTTI registration is a stock lazy `CClass::Ctor(&desc, "questNodeDefinition", 0x48, 3)`
  (e.g. `0x141456d90` for `questNodeDefinition`, `0x1415bb028` for
  `questSignalStoppingNodeDefinition`), enqueued into the global RTTI registrator. A plugin
  class registers exactly the same way and slots into this hierarchy.

> **`questISignalStoppingNodeType` exists but is irrelevant here** — it's a node-*type*
> interface (`ISignalStoppingNodeType : IRetNodeType`), the embedded-object axis. Dynamic
> replay nodes live on the **definition** axis, so we never touch the type interface.

### Conditions (polled by condition/pause nodes — only relevant if you reuse a stock wait)
```
graph::IGraphNodeCondition
└─ quest::IBaseCondition                 "questIBaseCondition"     size 0x30
   └─ quest::Condition  →  quest::TypedCondition  →  quest::FactsDBCondition  +Handle<IFactsDBConditionType> type @0x30
```
Time-based delays are conditions too: `questRealtimeDelay_ConditionType` /
`questTickDelay_ConditionType`, used with a pause/condition node to wait N seconds / N ticks.
A **custom yielding node definition does not require any condition object** — it builds its
own listener directly (§4/§6).

---

## 2. Execution dispatch — the signal pump and `ExecuteNode`

Quest execution is **signal propagation**, driven by a queue, not per-frame polling:

```
QuestPhaseInstance_ExecuteGraph (0x1409481d8)
  └─ signal pump  sub_14058E31C (0x14058e31c)   // drains a signal queue until empty
       └─ signal step  sub_14058E444 (0x14058e444)   // one signal → one node visit
            └─ QuestPhaseInstance_ExecuteNode (0x14058e5a8)   // pre/exec/post wrapper
                 └─ sub_1402595F0   // calls node->vtbl[0x158] == ExecuteNode (slot 43)
```

`QuestPhaseInstance_ExecuteNode` (simplified):
```c
char QuestPhaseInstance_ExecuteNode(phase, node, ctx, inSocket, outSockets) {
    // 1. walk the parent/"cut" chain; if cut → return 1
    // 2. node->vtbl[0x150](node, ctx+0x110, inSocket)   // slot 42 pre-pass (socket enum)
    //      if it returns 1 → return 1
    // 3. ret = sub_1402595F0(node, ctx+0x110, inSocket, outSockets)  // → node->vtbl[0x158] ExecuteNode
    // 4. node->vtbl[0x160](node, ctx+0x110, ret, outSockets)        // slot 44 post-pass
    return ...;
}
```

`sub_1402595F0` is the thin wrapper that reaches `ExecuteNode`:
```c
ctx->vtbl[0xE0](ctx, node->id);          // mark "entering node"
outSockets.len = 0;
return node->vtbl[0x158](node, ctx, inSocketName, outSockets);   // <-- ExecuteNode (slot 43)
```

### How the signal *continues* after a node — `sub_14058E444` (signal step)
This is the answer to "how does execution continue":
```c
char SignalStep(pump, signalSlot, phase) {
    node = resolve node from socket;
    if (visitCount(node) > 100) { log "Quest System: infinite loop detected … Signal died in: <node>"; return 1; }
    ret = QuestPhaseInstance_ExecuteNode(pump, node, phase, &outSockets, signalQueue);
    if (ret == 0) {                       // node DONE → forward the signal
        for (socket in outSockets)        //   outSockets filled by ExecuteNode
            if (socket != NULL_SOCKET)
                sub_140607C44(phase->signalQueue, edge(socket));   // enqueue connected next node(s)
        return 1;                         //   (this signal consumed; queued ones run next)
    }
    if (ret == 1) return 1;               // node SUSPENDED → signal dies here, nothing forwarded
    if (ret == 2) { /* embedded sub-phase */ ... }
}
```
- **`ExecuteNode` returns 0 → the step iterates the node's output sockets and `sub_140607C44`
  enqueues the connected next node(s)**; the pump then drains those. That is literally "the
  graph advances."
- **`ExecuteNode` returns 1 → the signal is not forwarded; it dies at the node.** The node is
  now dormant. Nothing re-runs it until an external event re-injects a signal (§4).
- The >100-visit guard is the engine's "Signal died in: …" infinite-loop trap — a node that
  keeps returning 1 without ever being woken would never log this (it isn't re-entered);
  the trap fires on cyclic *immediate* forwarding.

**`ExecuteNode` (slot 43, `0x158`) is the per-node behavior.** Signature matches the
project's existing binding:
`char ExecuteNode(NodeDefinition* this, QuestPhaseContext* ctx, CName inSocket, DynArray<CName>& outSockets)`.

### Per-class `ExecuteNode` (slot 43) — proof every node has its own, and the shared-hook problem
Scanning `.rdata` for node-definition vtables (slot 42 == `0x14058e78c`,
`DispatchSocketEnumerationByKind`) finds **81** named `quest*NodeDefinition::vtbl` objects.
A representative sample of their slot-43 `ExecuteNode`:

| Node class (`::vtbl`) | vtbl | slot-43 `ExecuteNode` | shape |
|---|---|---|---|
| `questNodeDefinition` (base)            | `0x142af9c70` | `0x14190706c` `purecall` | abstract — must override |
| `questFactsDBManagerNodeDefinition`     | `0x142af9640` | `0x1404321e8` | immediate, **SHARED** ↓ |
| `questDynamicSpawnSystemNodeDefinition` | `0x142e9b890` | `0x1404321e8` (same) | immediate, **SHARED** |
| `questRewardManagerNodeDefinition`      | `0x142bd6cf8` | `0x1404321e8` (same) | immediate, **SHARED** |
| `questPauseConditionNodeDefinition`     | `0x142af9ad8` | `0x1404605e4` (unique) | **yields** (§4) |
| `questCheckpointNodeDefinition`         | `0x142b90800` | `0x14181d128` (unique) | **yields** (own listener) |
| `questConditionNodeDefinition`          | `0x142af9318` | `0x140461858` (unique) | branch |
| `questFlowControlNodeDefinition`        | `0x142bfec90` | `0x14107fa3c` (unique) | own |
| `questFactsDBManagerNodeDefinition` family also shares with a *second* big "manager" execute `0x140563f88` used by `questItemManager`, `questEntityManager`, `questFXManager`, `questTimeManager`, … | | | immediate, shared within that family |

So hooking `0x1404321e8` (what `src/Comms/ReplayComms.cpp` does via
`shared::raw::Quest::FactsDBManager::Execute`) is unavoidably hit by **all three** of
FactsDBManager / DynamicSpawn / Reward — the issue's complaint, confirmed at the vtable
level. A **custom** node-definition class has its **own** vtable and its **own** slot-43
function, so its `ExecuteNode` is hit *only* by instances of that node — no hook, no sharing.

---

## 3. Immediate node-definition model (fire-and-forget command)

`0x1404321e8` (`FactsDBManager::Execute`, the immediate manager-node `ExecuteNode`,
currently hooked; hashes `3679006322u` / `3909955204u` / `4110823589u` all resolve to
offset `0001:004311e8`):
```c
char ExecuteImmediate(node, ctx, inSocket, outSockets) {
    type = node->[+0x48];               // Handle<…NodeType> type
    if (type) type->vtbl[0xE8](type);   // node-type Execute (slot 29) — the per-type work
    emit default output socket;         // sub_1401943F4 appends to outSockets
    return 0;                           // ALWAYS done → forwards immediately, cannot yield
}
```
The stock immediate node delegates its work to an embedded `type` object. A **custom
immediate node definition** skips the embedded type entirely: override `ExecuteNode` to do
the work inline off `this`, then emit output and return 0.
```c
// questReplayNodeDefinition : questDisableableNodeDefinition   (immediate variant)
char ReplayNode::ExecuteNode(self, ctx, inSocket, outSockets) {
    FireNativeCommand(self->command, self->payload);   // typed fields straight off `this`
    EmitOutputSocket(outSockets);                      // reuse sub_1401943F4
    return 0;                                           // advance now
}
```
This is the cleanest replacement for the current `SetVar`/fact-name protocol for the
fire-and-forget half (`INITIALIZE_ALL`, `FINISHED`). It does **not** wait for an ack.

### (Reference) the node-*type* `Execute` contract (slot 29, `0xE8`)
For completeness — this is the embedded-object axis the stock managers use, reversed from
`questSetVar_NodeType::Execute` (`0x140432230`, hash `885529461`, type vtbl `0x142b7f480`):
```c
void NodeType::Execute(NodeType* this, QuestPhaseContext* ctx) {
    // SetVar: FNV1a32(this->factName); questObj = ctx->[+0x28]; SetFact(hash, this->value, this->setExactValue)
}
```
A custom node *definition* makes this indirection unnecessary; it is listed only to show
where the stock `SetVar` work happens.

---

## 4. Yielding node-definition model — `questSignalStoppingNodeDefinition`

`questPauseConditionNodeDefinition::ExecuteNode` (`0x1404605e4`, hash `457322231`, slot 43,
unique to pause nodes) is the canonical "non-immediately-executing node". A custom yielding
node reproduces this shape; the register/wake plumbing it calls is **inherited** from
`SignalStopping` + `QuestPhaseContext`:
```c
char PauseCondition_ExecuteNode(node, ctx, inSocket, outSockets) {
    cond = node->[+0x48];                          // Handle<IBaseCondition> condition
    if (!cond) { emit output; return 0; }          // no condition → pass through

    satisfied = cond->vtbl[0xF8](cond);            // IBaseCondition evaluate (slot 31)
    nodeId    = node->id;                           // +0x40

    if (satisfied) {
        if (ctx->vtbl[0x80](ctx, nodeId)) {         // IsNodePending?  (was it suspended)
            ctx->vtbl[0x68](ctx, nodeId);           // UnregisterPendingNode → tears down the listener
            if (ctx->vtbl[0x110](ctx) == 1)         // phase still active?
                phaseOwner->vtbl[0x298](phaseOwner, descriptor);   // notify "node resumed"
        }
        emit output; return 0;                      // CONDITION MET → advance graph
    }

    // condition NOT yet satisfied:
    if (!ctx->vtbl[0x80](ctx, nodeId))              // not already registered?
        SignalStopping_RegisterPending(node, ctx);  // 0x1402581e0 → build+register the listener
    return 1;                                        // YIELD; signal dies; node now dormant
}
```
`questCheckpointNodeDefinition::ExecuteNode` (`0x14181d128`) is the sibling yielding node and
uses the same `SignalStopping_RegisterPending` path with its own slot-48/49 listener — proof
the suspend mechanism is the *family's*, shared across `SignalStopping` subclasses.

---

## 4A. The suspend / resume lifecycle (the core mechanism)

**Execution is signal-propagation.** A signal enters a node; the node runs once; if it
returns 0 it forwards the signal to its outputs (§2); if it returns 1 the signal dies and
the node is **woken only by its own listener**. No per-tick "re-poll all waiting nodes" loop
exists.

### Objects involved
- **`QuestPhaseContext`** (vtable `off_142AC3138`): the `ctx` every `ExecuteNode` receives.
  Relevant virtuals:
  | slot | off | fn | role |
  |---|---|---|---|
  | 12 | `0x60` | `0x140257ff8` | **RegisterPendingNode**(nodeId, signalName, descriptor) |
  | 13 | `0x68` | `0x14025969c` | UnregisterPendingNode(nodeId) |
  | 16 | `0x80` | `0x140256438` | IsNodePending(nodeId) |
  | 20 | `0xa0` | `0x140257a08` | get pending descriptor for nodeId |
- **active `QuestPhaseInstance`** = `sub_140257A30(ctx)` (top of the phase stack). The
  **pending-node map lives at `phaseInstance + 152`**, keyed by node id, valued by the
  listener descriptor.
- **listener descriptor** (the wake object; FactsDB variant vtbl built via `0x140259f0c`).
  Relevant slots: **`0x10` Activate** (subscribe to event source), **`0x60` OnFired**
  (event callback). Fields: `+0x18` owner (re-fire target), `+0x20` parent (nesting),
  `+0x28` listener index, `+0x44` cached "satisfied" flag, `+0x50` watched key/token.

### Step 1 — Suspend (`ExecuteNode` returns 1)
`SignalStopping_RegisterPending` (`0x1402581e0`, hash `2048335859`, shared by all yielding
nodes):
```c
void RegisterPending(node, ctx /*QuestPhaseContext*/) {
    descriptor = node->vtbl[0x188](node, …);   // slot 49 BuildListener
    name       = node->vtbl[0x180](node, …);   // slot 48 GetSignalName
    ctx->vtbl[0x60](ctx, node->id, *name, descriptor);   // RegisterPendingNode
}
```
For the pause/condition family, slots 48/49 delegate to the condition handle at +0x48:
- slot 48 `GetSignalName` = `0x140259390` (hash `2751022175`) → `condition->vtbl[0x118]`
- slot 49 `BuildListener` = `0x1402593b0` (hash `2902212628`) → `condition->vtbl[0xE8]`

`RegisterPendingNode` (`0x140257ff8`, hash `4234888014`) stores `descriptor` in
`phaseInstance+152[nodeId]`, records the listener index back into the descriptor (`+0x28`),
then **activates** it: `descriptor->vtbl[0x10](descriptor, ctx)` — the listener subscribes
to its event source *now*. The node is dormant.

For `questFactsDBCondition`, `BuildListener` (`0x140259f0c`, hash `1975926336`) delegates to
the inner `questIFactsDBConditionType` (`type->vtbl[0xD8]`), whose listener **subscribes to
the FactsDB change signal for the specific fact**. So the wake event for a fact wait is
literally "that fact was written" — event-driven and cheap. Time conditions arm a
timer/tick listener instead; same shape, different source.

### Step 2 — Resume (event fires → re-inject signal → re-run node)
The wake chain, reversed from the FactsDB listener:
```c
// desc vtbl slot 12 / +0x60
Listener::OnFired(desc) {                         // 0x14045eca8  (hash 1621955682)
    cur = factsDb->vtbl[0x10](factsDb, 1, desc->[+0x50]);   // re-read the watched fact
    satisfied = compare(cur, desc->[+0x44 cmp]);            // re-evaluate
    Listener::SetState(desc, satisfied);
}
Listener::SetState(desc, satisfied) {             // 0x14045ec28  (hash 4263842581)
    if (atomic_edge_change(desc->[+0x44], satisfied))       // only on a real flip
        Listener::Refire(desc);
}
Listener::Refire(desc) {                          // 0x14045fa44  (hash 776148020)
    root = desc; while (root->[+0x20] /*parent*/) root = root->[+0x20];
    root->[+0x18] /*owner*/ ->vtbl[0x08](owner, root->[+0x28] /*index*/);   // RE-INJECT quest signal
}
```
`owner->vtbl[0x08]` is the generic "wake the suspended node" entry: it re-injects a signal
that re-enters the phase pump (§2). The node's `ExecuteNode` runs again, now sees the
condition satisfied, `UnregisterPendingNode`s (tearing down the listener), emits its output
socket, and returns 0 — at which point the signal step (`sub_14058E444`) forwards the signal
to the next node and the graph advances. **This is exactly how a node "continues execution
after the signal-stopping completes": its own `ExecuteNode`, on the resume pass, returns 0,
and the step loop forwards from its output socket.**

`Listener::SetState`/`Refire` only touch descriptor fields + `owner->vtbl[0x08]`, so a custom
node can **reuse them verbatim** — the only new code is `Activate` (subscribe to our wake
source) and the `OnFired`/flag logic (set "acked = true").

### Why this matters for replay
- **Stock wait, zero native code:** a stock `questPauseConditionNodeDefinition` →
  `questFactsDBCondition(replay_init_finished == 1)` yields until `FactsDBManager::SetFact`
  flips the fact (which `ReplayManager` already does). The FactsDB change signal wakes the
  listener → re-check → advance. No hook, no polling, no state held across the wait.
- **Custom yielding node** = the reusable primitive (§6): derive from
  `questSignalStoppingNodeDefinition`, override `ExecuteNode` + the listener hooks, drive the
  wake from native code. More work than the stock wait, but it carries a typed command and
  needs no facts.

> Save/load + `.scene` rewind/fast-forward interaction with suspended nodes is explicitly out
> of scope for replay (author note). A stock `PauseCondition + FactsDBCondition` uses the same
> persistence path the base game relies on, so it inherits whatever the engine does there; a
> custom node would need a smoke test.

---

## 5. Condition evaluation — `questFactsDBCondition` (only if you reuse a stock wait)

`IBaseCondition::Evaluate` is slot **31** (`0xF8`). For the `questCondition` family it's
`sub_140259f98` (hash `3542489730`), which delegates to a typed sub-condition:
```c
char Condition_Evaluate(cond, ctx) {            // 0x140259f98
    inner = cond->vtbl[0x170](cond);            // typed condition object
    return inner ? inner->vtbl[0x108](inner, ctx)  // typed evaluate (slot 33)
                 : 1;                            // null type → trivially true
}
```
A `questFactsDBCondition` carries `Handle<questIFactsDBConditionType> type` (@0x30) that
holds the "which fact / which comparison" data. **A custom yielding node does not need any of
this** — it owns its ack-state directly. This section stands only for the stock-wait shortcut
of §4A.

---

## 6. What "dynamic quest node" means here (corrected)

WolvenKit's quest graph editor lets a node's **class name** be any class registered in RTTI,
including one the base game does not ship. The engine's quest-resource loader looks the class
up by name, allocates `CClass.size` bytes, constructs the instance (which writes the class's
C++ vtable pointer), deserializes its authored fields, and from then on dispatches it through
its **own vtable** — `ExecuteNode` at slot 43, exactly like every stock node. So:

- **A dynamic node is a custom *node-definition* class**, e.g.
  `questNodeDefinition → questSignalStoppingNodeDefinition → questReplayNodeDefinition`. It is
  the same kind of object as `questPauseConditionNodeDefinition`/`questCheckpointNodeDefinition`.
  Its typed fields (`CName command`, `Variant payload`, `u32 token`) are authored in WolvenKit
  and read straight off `this`. No embedded `type`/`condition` object is required.
- **Immediate vs. yield is chosen by which base you derive from and what `ExecuteNode`
  returns** — derive from `questDisableableNodeDefinition` and return 0 for fire-and-forget
  (§3); derive from `questSignalStoppingNodeDefinition` and return 1 to suspend (§4). You do
  *not* graft a custom type onto a stock node.
- **Native vs. script registration is the real subtlety.** The engine dispatches node
  execution purely through the C++ object vtable (`sub_1402595F0`: `node->vtbl[0x158]`) — no
  RTTI/script indirection on the hot path. Therefore:
  - A class registered as a **native** type (RED4ext, with a real C++ vtable that mirrors the
    node-definition layout) is dispatched correctly — the engine calls *your* `ExecuteNode`.
  - A class registered as a **script/`IScriptable`-backed** type gets the `IScriptable`
    vtable, whose slot 43 is **not** `ExecuteNode`; the engine would call the wrong function.
    This is the hazard — it is specific to *script* registration, not to "registering a custom
    class" in general.
  So the work is: register a native class **and present the node-definition C++ vtable**.
  Because the SDK's generated headers are layout mirrors (not `virtual`-method declarations),
  the vtable must be supplied explicitly — copy the stock base vtable and overwrite the slots
  you implement (§6.1). `psiberx/cp2077-codeware`/`archive-xl` are the precedent for native
  classes with custom vtables.

### 6.1 Vtable you must present (node-definition, 60 slots; base = `questSignalStoppingNodeDefinition`)
Build a static `void*[N]` in the plugin; **memcpy the stock base vtable** (so serialization /
socket / RTTI plumbing keeps working) then overwrite only:
| slot | off | implement | notes |
|---|---|---|---|
| 43 | `0x158` | **`ExecuteNode`** | the whole immediate-or-suspend decision (§3 / §4) |
| 49 | `0x188` | **`BuildListener`** | allocate your listener descriptor, return it (yield only) |
| 48 | `0x180` | **`GetSignalName`** | return the `CName` to register the listener under (yield only) |
| ISerializable block | | (de)serialize your typed fields | so authored `command`/`payload`/`token` survive load |

Inherit/copy from the base vtable everything else: slot 0/1 `GetType`, slot 42 pre-pass
(`DispatchSocketEnumerationByKind`, `0x14058e78c`), slot 44 post-pass (`ReturnInputBool`,
`0x14058fc34`), and the socket-management slots — touching those is what makes the node show
its in/out sockets and survive save/load. For the **immediate** variant you only override slot
43 (+ serialization); slots 48/49 are unused.

---

## 7. Answers to issue #1's five questions

1. **Lifecycle / immediate vs yield.** Every node is a `questNodeDefinition` subclass; its
   slot-43 `ExecuteNode` runs once per signal visit. `return 0` → emit output, signal step
   forwards to the next node (advance); `return 1` → suspend (register as a listener via
   `ctx->vtbl[0x60]`, signal dies) until an event re-injects a signal. Immediate nodes
   (`Disableable`/manager) always return 0; the `SignalStopping` family yields.
2. **Typed payload.** Yes — author typed fields (`CName command`, `Variant payload`, `u32
   token`) directly on the **custom node definition** and read them off `this` in
   `ExecuteNode`. No fact-name string protocol, and no embedded type object. (The stock
   `questSetVar_NodeType`/`questFactsDBCondition` typed sub-objects show the same idea on the
   embedded-object axis, which we don't need.)
3. **Authoring story.** WolvenKit places a node and sets its class name to our registered
   plugin class. The plugin registers that class (native, with the node-definition vtable)
   before the quest resource loads.
4. **Re-entry semantics.** Handled by the `SignalStopping` listener table (§4/§4A): the quest
   system suspends/resumes the node; only `QuestPhaseContext` + the owning phase instance need
   to persist, and the engine already owns them. The replay handshake maps onto either a stock
   `PauseCondition + FactsDBCondition` wait or our custom listener.
5. **Hook retirement.** The shared `FactsDBManager::Execute` hook is retired by moving commands
   onto a dedicated **custom node**:
   - **Immediate command node** (§3) for fire-and-forget — retires the hook outright.
   - **Yielding node** (§4/§6) when an ack/wait is needed.
   Either removes the DynamicSpawn/Reward collision because a custom class has its own vtable
   and its own `ExecuteNode`.

---

## 8. Recommendation for the replay migration

- **Wait-until-ack (`ReplayInitNode`):**
  - *Interim, zero native code:* stock `questPauseConditionNodeDefinition` →
    `questFactsDBCondition(replay_init_finished == 1)`. Native side sets the fact at the end of
    `SetupQuestState/PlayerData/Inventory`; the node resumes on the FactsDB change signal.
    Strictly better than today's SetVar + hook-watches-the-fact arrangement for the *wait* half.
  - *Target:* custom `questReplayNodeDefinition : questSignalStoppingNodeDefinition` (§6) that
    fires the typed command on first visit and yields until native code wakes its listener —
    no facts at all.
- **Quest→native command (`INITIALIZE_ALL` / `FINISHED`):** custom
  `questReplayNodeDefinition : questDisableableNodeDefinition` immediate node (§3) carrying a
  `CName command` + optional `Variant payload`. Retires the shared hook; the only cost is the
  node-definition vtable (§6.1).
- Centralize the command vocabulary (`INITIALIZE_ALL`, `FINISHED`, future `checkpoint`) as a
  `CName` field on the node, not a fact-name prefix.
- **Sequencing:** ship the immediate command node first (small, retires the hook), keep the
  stock `PauseCondition + FactsDBCondition` for the wait, then promote the wait to a custom
  yielding node (§11) only when a second consumer needs general non-immediate nodes.

---

## 9. Address / hash table (for `RawFunc` / `UniversalRelocFunc` bindings)

Offsets are `section:rva-from-section-base` (`.text` = section `0001`, base RVA `0x1000`).
Every hash below was recomputed from the function RVA with the `cp2077-address-hash` skill and
matched `cyberpunk2077_addresses.json` for this build.

| Symbol (suggested) | Addr (IDB) | offset | address-hash | Role |
|---|---|---|---|---|
| `QuestPhaseInstance_ExecuteGraph` | `0x1409481d8` | `0001:009471d8` | `779107884` | top of quest graph execution |
| `QuestPhaseInstance_SignalPump` (`sub_14058E31C`) | `0x14058e31c` | `0001:0058d31c` | `855850164` | drains the signal queue |
| `QuestPhaseInstance_SignalStep` (`sub_14058E444`) | `0x14058e444` | `0001:0058d444` | `630663353` | one signal → node visit; **forwards output sockets on ret 0** |
| `QuestPhaseInstance_ExecuteNode` | `0x14058e5a8` | `0001:0058d5a8` | `3227858325` | pre/exec/post wrapper (already bound) |
| `NodeDefinition::ExecuteNode` dispatch (`sub_1402595F0`) | `0x1402595f0` | `0001:002585f0` | — | calls slot-43 `ExecuteNode` |
| `EnqueueSignalEdge` (`sub_140607C44`) | `0x140607c44` | `0001:00606c44` | `3979943455` | enqueues connected next node(s) |
| `EmitOutputSocket` (`sub_1401943F4`) | `0x1401943f4` | `0001:001933f4` | `3919059478` | append a socket to `outSockets` |
| `FactsDBManager::Execute` (shared immediate `ExecuteNode`) | `0x1404321e8` | `0001:004311e8` | `3679006322` / `3909955204` / `4110823589` | **currently hooked**; FactsDB/DynamicSpawn/Reward |
| `questSetVar_NodeType::Execute` (type slot 29) | `0x140432230` | `0001:00431230` | `885529461` | reference: sets a fact |
| `questPauseConditionNodeDefinition::ExecuteNode` (slot 43) | `0x1404605e4` | `0001:0045f5e4` | `457322231` | yields until condition true |
| `questCheckpointNodeDefinition::ExecuteNode` (slot 43) | `0x14181d128` | `0001:0181c128` | `1283075410` | sibling yielding node (suspends via `SignalStopping_RegisterPending`) |
| `quest::Condition::Evaluate` (slot 31) | `0x140259f98` | `0001:00258f98` | `3542489730` | `IBaseCondition` eval |
| `SignalStopping_RegisterPending` | `0x1402581e0` | `0001:002571e0` | `2048335859` | suspend: build+register listener |
| `QuestPhaseContext::RegisterPendingNode` (slot 12) | `0x140257ff8` | `0001:00256ff8` | `4234888014` | store at `phaseInst+152`, Activate descriptor |
| `SignalStopping::GetSignalName` (slot 48, cond-delegating) | `0x140259390` | `0001:00258390` | `2751022175` | → `condition->vtbl[0x118]` |
| `SignalStopping::BuildListener` (slot 49, cond-delegating) | `0x1402593b0` | `0001:002583b0` | `2902212628` | → `condition->vtbl[0xE8]` |
| `questFactsDBCondition::BuildListener` (slot 29) | `0x140259f0c` | `0001:00258f0c` | `1975926336` | subscribes to FactsDB change = wake source |
| `Listener::OnFired` (desc slot 12) | `0x14045eca8` | `0001:0045dca8` | `1621955682` | event callback → re-evaluate |
| `Listener::SetState` | `0x14045ec28` | `0001:0045dc28` | `4263842581` | atomic flag @desc+0x44; edge → Refire |
| `Listener::Refire` | `0x14045fa44` | `0001:0045ea44` | `776148020` | `owner->vtbl[0x08]` re-injects signal |

Object vtables: `questNodeDefinition::vtbl` `0x142af9c70` (slot 43 = `purecall`);
`questPauseConditionNodeDefinition::vtbl` `0x142af9ad8`;
`questCheckpointNodeDefinition::vtbl` `0x142b90800`;
`questFactsDBManagerNodeDefinition::vtbl` `0x142af9640`;
`questFactsDBCondition::vtbl` `0x142b73790`; `questSetVar_NodeType::vtbl` `0x142b7f480`.
`QuestPhaseContext::vtbl` `off_142AC3138`.

Key object-vtable slots (node definition): `0x150` slot 42 pre-pass ·
**`0x158` slot 43 ExecuteNode** · `0x160` slot 44 post-pass · `0x180` slot 48 GetSignalName ·
`0x188` slot 49 BuildListener (last two used by the signal-stopping family).
`QuestPhaseContext` virtuals used by yield: `0x60` RegisterPendingNode · `0x68` Unregister ·
`0x80` IsNodePending · `0xa0` get-descriptor.
Listener descriptor: `0x10` Activate · `0x60` OnFired; fields `+0x18` owner · `+0x20` parent ·
`+0x28` index · `+0x44` cached flag · `+0x50` watched key/token.

---

## 10. Open / `[verify in-game]`

- **Native class registration + vtable construction (§6.1)** — confirm against
  `psiberx/cp2077-codeware`: (a) how it registers a native class so the quest-resource loader
  instantiates it by class name with the plugin's vtable (compare a stock `*::ConstructCls`),
  and (b) the exact 60-slot node-definition vtable layout to copy from the
  `questSignalStoppingNodeDefinition` base. Only slots 43/48/49 + the ISerializable block are
  ours; the rest must be copied so sockets/serialization work.
- **`questCheckpointNodeDefinition::ExecuteNode` (`0x14181d128`, hash `1283075410`)** —
  *confirmed* the non-condition signal-stopping pattern: it suspends via the same
  `SignalStopping_RegisterPending` (`0x1402581e0`) → `return 1`, resumes via emit-output →
  `return 0`, and keys off its own checkpoint state (field @+0x48) + its own slots 48/49
  rather than a `condition` handle. This is the closest stock analogue to a custom yielding
  node — a model to copy.
- **Listener owner re-fire (`owner->vtbl[0x08]`)** — confirm the owner at descriptor `+0x18`
  is the phase instance (not a sub-object) and that calling it off the quest tick is safe;
  prefer driving native wakes from the quest/`Tick` thread.
- **Save/load + `.scene` rewind** interaction with a custom yielding node — out of scope for
  replay, but smoke-test before shipping one.

---

## 11. Draft — building a *fully custom* yielding quest node (reusable)

Goal: a plugin node that **fires a typed native command** *and* **yields until native code
resumes it**, without any FactsDB condition. With the corrected model this is one class, not
three:

### 11.1 Object model
```
questReplayNodeDefinition : quest::SignalStoppingNodeDefinition   (the node, in the graph)
   └─ typed fields: CName command, Variant payload, u32 token   (authored in WolvenKit, read off `this`)
   └─ ExecuteNode (slot 43): fire command once, then suspend(1); on resume emit output(0)
   └─ BuildListener (slot 49) / GetSignalName (slot 48): hand the engine our listener
ReplayNodeListener : <engine listener-descriptor shape, ~88 bytes>
   └─ Activate (slot 0x10): register {token → this} in a plugin wake-registry (our event source)
   └─ OnFired/SetState/Refire: reuse engine SetState (0x14045ec28) + Refire (0x14045fa44) verbatim
ReplayWakeRegistry  (plain plugin singleton, NOT RTTI): token → live listener; native code calls Resume(token)
```
No separate `condition`/`type` object — the node owns the payload and builds the listener
itself (slots 48/49).

### 11.2 `ExecuteNode` (slot 43) — suspend/resume body
```c
char ReplayNode::ExecuteNode(self, ctx /*QuestPhaseContext*/, inSocket, outSockets) {
    if (IsResumed(self->token)) {                 // native side already acked?
        if (ctx->vtbl[0x80](ctx, self->id))       // IsNodePending
            ctx->vtbl[0x68](ctx, self->id);       // UnregisterPendingNode (tears down listener)
        EmitOutputSocket(outSockets);             // sub_1401943F4
        return 0;                                 // advance → signal step forwards to next node
    }
    if (!ctx->vtbl[0x80](ctx, self->id)) {        // first visit (not yet suspended):
        FireNativeCommand(self->command, self->payload);   // typed quest→native call, once
        SignalStopping_RegisterPending(self, ctx);// 0x1402581e0: BuildListener(49)+GetSignalName(48)+register+Activate
    }
    return 1;                                      // yield
}
```
`SignalStopping_RegisterPending` is reused as-is: it calls our slot 49/48, hands the listener
to `RegisterPendingNode` (`0x140257ff8`), which stores it at `activePhaseInstance+152[nodeId]`
and calls our `Activate`.

### 11.3 Listener `Activate` + wake
```c
void ReplayNodeListener::Activate(self, ctx) {     // slot 0x10  (engine FactsDB: subscribe to facts)
    g_replayWakeRegistry.insert(self->token, self); // OUR event source
    // optional: immediate check in case the ack already happened
}
// native code, when replay setup finishes:
void ReplayWakeRegistry::Resume(token) {           // from ReplayManager request-drain (quest/tick thread)
    if (auto* l = lookup(token)) { SetState(l, true); }   // 0x14045ec28 → edge → Refire (0x14045fa44)
}
```
`SetState`→`Refire` walk to the root descriptor and call `owner(+0x18)->vtbl[0x08](owner,
index@+0x28)` — re-injecting the quest signal so the phase pump re-runs our `ExecuteNode`,
which now sees `IsResumed`, unregisters, emits output, returns 0. We reuse the engine's
`SetState`/`Refire` verbatim; the only new code is `Activate` (register) and `Resume` (flip
the flag).

### 11.4 Threading & integration
- `FireNativeCommand` / `Resume` run on the quest job thread. Keep the existing discipline:
  `FireNativeCommand` only **enqueues** a `ReplayManager` request; the request drain on
  `PostBuckets` does the real work and, when done, calls `ReplayWakeRegistry::Resume(token)`,
  which pokes the quest system from the tick thread.
- `token` = a per-node `u32`/`CName` authored in WolvenKit → one node instance ↔ one wake.
- Retires the `FactsDBManager` hook entirely: the command rides on the node's typed fields,
  the wait rides on our listener — neither uses facts.

### 11.5 Why this is still real work (and the cheaper alternative)
The genuine cost is the **node-definition C++ vtable** (§6.1) plus a small listener-descriptor
vtable, and registering the class **native** so the loader constructs it with our vtable (a
script/`IScriptable` registration gets the wrong vtable — §6). It is one definition vtable
deriving from `SignalStopping`, not three from scratch, and no embedded type/condition object.
If the only near-term need is the replay handshake, the §4A stock `PauseCondition +
FactsDBCondition` wait + a custom **immediate** command node (§3) gets ~90% of the value for a
fraction of the risk. Build §11 when a second consumer needs general non-immediate nodes.

### 11.6 Open items for §11 `[verify in-game]`
- Confirm the **owner** at descriptor `+0x18` is the phase instance and its **slot `0x08`** is
  the re-inject entry (read off the FactsDB listener); verify calling it off-tick is safe.
- Confirm descriptor field offsets (`+0x28` index, `+0x44` flag, `+0x18` owner, `+0x50` token)
  on the current build — they came from `0x14045ec28`/`0x14045fa44`/`0x14045eca8`.
- Confirm the native registration + vtable sequence so the quest loader instantiates
  `questReplayNodeDefinition` by class name with **our** vtable (compare a stock
  `*::ConstructCls` and codeware's native-class registration).

### 11.7 Draft SDK headers
Buildable RED4ext.SDK-style headers that reproduce the node-definition vtable (so a C++
subclass of `questSignalStoppingNodeDefinition` gets the engine's vtable) live under
`drafts/quest-node-vtables/` — `questNodeDefinition.hpp` + `-inl.hpp` (slots 27–49,
`ExecuteNode` @ 43, forwarders bound by the §9 hashes), the chain links, and an example
`questReplayNodeDefinition` + listener. See that folder's `README.md` for placement and the
construction caveat. They're modelled on the SDK's hand-authored `IGameSystem`/`ISerializable`
(declare virtuals with offset comments; define them in `-inl` as defaults or
`UniversalRelocFunc` forwarders), so they can be PR'd to RED4ext.SDK after in-game validation.
