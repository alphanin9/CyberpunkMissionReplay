# Mission Replay — Implementation Plan

> Status: planning document. Drafted 2026-05-31.
> Scope: how to continue building out the replay mechanic on top of the
> current RED4ext/RedLib/SharedPunk native plugin and the WolvenKit quest
> project.
>
> Environment note: Cyberpunk 2077 is **not installed on this machine** and no
> game binary is available, so nothing below is grounded in IDA. Claims about
> engine internals (offsets, system lifetimes, telemetry layout) are marked as
> **[verify in-game]** where they need confirmation against a live build,
> NativeDB/Cyberdoc, or the decompiled scripts.

---

## 1. Design recap

### 1.1 The core constraint

Quests in CP2077 mutate an enormous amount of distributed game state (FactsDB,
journal, persistent object state, scriptable systems, world streaming, NPC
populations, etc.). There is **no reliable "undo"** for a quest that has
already run. Rolling the player's live session back to an earlier mission is
therefore off the table.

### 1.2 The chosen model: clean-slate quest trees

Instead of rewinding, each replayable mission is shipped as a **self-contained
quest tree starting from a clean slate**:

- A custom `gsmGameDefinition` (`*.gamedef`) per mission, pointing at a small
  custom `*.quest` graph plus the Night City world and a spawn tag.
- The `*.quest` graph contains only the phases needed for *that* mission
  (`*.questphase` files), wired together standalone — see
  `wolvenkit/source/archive/mod/quest/replay/`.
- Launching a replay starts a **brand new game session** from that gamedef
  (like "New Game", but with a stripped quest graph and the player's preserved
  appearance / build / inventory), rather than resuming the player's save.

The replays already scaffolded in the WolvenKit project:

| Folder      | Mission(s)                          | Notes                          |
|-------------|-------------------------------------|--------------------------------|
| `q113`      | Devil ending                        | gamedef + quest                |
| `q115`      | Rogue ending                        | gamedef + quest                |
| `q306`      | PL Songbird path                    | gamedef + quest + questphase   |
| `boss_rush` | composite: q003 (Royce), q110 (mall)| gamedef + quest + phases       |
| `smasher`   | Adam Smasher fight                  | questphase util                |

### 1.3 The lifecycle as currently wired

```
[live session / main menu]
        │  (player picks a mission + options — UI layer NOT YET BUILT)
        ▼
ReplayManager::StartReplayGameDefinition(def)         src/Manager/ReplayManager.cpp:209
        │  - reads CharacterCustomizationSystem state  (offset 0x78)   [verify in-game]
        │  - SessionLoader::LoadGameDefinitionByPath(...)
        ▼
SessionLoader builds GameSessionDesc from the gamedef  src/Session/SessionLoader.cpp:40
        │  - world id, main quests, spawn tags, char customization
        │  - ink SystemRequestsHandler::StartSession(...)
        ▼
[new clean-slate replay session loads]
        │  quest graph runs; at its start it sets a FactsDB var
        │  "REPLAY_CALL_HOOK_INITIALIZE_ALL"
        ▼
ReplayComms FactsDBManager::Execute hook              src/Comms/ReplayComms.cpp:28
        │  recognizes the REPLAY_CALL_HOOK_ prefix, queues ReplayStarted
        ▼
ReplayManager::Tick → ReplayStarted                   src/Manager/ReplayManager.cpp:68
        │  SetupQuestState()  → apply staged facts
        │  SetupPlayerData()  → (currently) apply a static progression build
        │  SetupInventory()   → (empty)
        │  FactsDB.SetFact("replay_init_finished", 1)  → unblocks the quest
        ▼
[mission plays out]
        │  quest end sets "REPLAY_CALL_HOOK_FINISHED"
        ▼
ReplayComms hook → queues ReplayEnded → Tick          src/Manager/ReplayManager.cpp:85
        │  ink ExitToMenu()   // TODO: instead restore the player's prior save
        ▼
[back to menu]   // round-trip to the original save is NOT YET implemented
```

---

## 2. Current state of the code

### 2.1 What works / exists

- **Plugin skeleton** (`src/main.cpp`): RedLib `TypeInfoRegistrar::RegisterDiscovered()`
  on `Load`. Standard RED4ext entrypoints.
- **`ReplayManager`** (`IGameSystem`) with RTTI exports:
  - `StartReplayGameDefinition(EReplayGameDefinition)` — launches a gamedef and
    preserves character customization.
  - `SetQuestState(array<ReplayFactDefinition>)` — stages facts to apply at init.
- **`ReplayFactDefinition`** scriptable — a `(factName, factValue)` pair, RTTI-exposed.
- **`SessionLoader`** — turns a `gsmGameDefinition` into a `GameSessionDesc` and
  starts an ink session; also `LoadSavedGameByName`. A `SessionWrapper_TEST`
  class exposes manual test entrypoints to RTTI.
- **`ReplayComms`** — quest→native channel via a `FactsDBManager::Execute` hook,
  using the `REPLAY_CALL_HOOK_` fact-name convention with `INITIALIZE_ALL` /
  `FINISHED` commands.
- **Request queue** — thread-safe `AddRequest` + `Tick` draining on the
  `PostBuckets` update group, so quest-thread callbacks hand work to a known
  tick stage.
- **`SetupPlayerData`** — applies a static debug progression build
  (`ProgressionBuilds.VHard_50_RefBody`) via `quest::SetProgressionBuildRequest`
  queued onto `PlayerDevelopmentSystem`.
- **`CapturePointOfNoReturnId`** — reads the telemetry system's PONR id string
  (offset `0xD8` inside the data container at offset `184`) **[verify in-game]**.
- **WolvenKit quest project** — gamedefs/quests/phases for the missions above.

### 2.2 What is stubbed / missing (the work)

| Area | Symbol / location | Gap |
|------|-------------------|-----|
| Mission selection UI | (none) | No REDscript/ink layer calls `StartReplayGameDefinition` / `SetQuestState`. No mission catalog surfaced to the player. |
| Source-session capture | (none) | Only character customization is captured. Player **build** and **inventory** are not captured from the live session before the replay launches. |
| Cross-session persistence | `m_pointOfNoReturnId` etc. | Replay is a *separate session*; a game-system instance does not survive the session swap, so anything captured must live outside the per-session system or be serialized. **[verify in-game]** |
| PONR / save round-trip | `ReplayManager.cpp:87,93,119` | `CapturePointOfNoReturnId` is never called; `m_pointOfNoReturnId` is never persisted or used; ReplayEnded just exits to menu instead of loading the player's prior save. |
| `SetupQuestState` content | `ReplayManager.cpp:141` | Applies whatever facts were staged, but there is no per-mission **preset** of the facts each mission needs (Takemura/Oda/Jackie etc. per the header comment). |
| `SetupPlayerData` | `ReplayManager.cpp:159` | Hard-coded static build only; **target is to preserve the player's real progression** (capture → re-apply). Static build demoted to a debug fallback. |
| `SetupInventory` | `ReplayManager.cpp:192` | Empty; **target is to preserve the player's real inventory**. |
| Preset save/load | (none) | No way to snapshot a player-state + options set to disk and reload it later as a named preset. |
| Comms strategy | `ReplayComms.cpp:13` | Header note: dynamic quest nodes now partly supported, FDBManager hook may be replaceable. Decision pending. |
| Loading screen | `SessionLoader.cpp` (commented) | Custom replay loading screen TDBID disabled. |
| `EReplayGameDefinition` coverage | `ReplayManager.cpp:196` | Enum/`GetGameDefinition` only map Q113/Q115; Q306, boss_rush, smasher gamedefs exist on disk but aren't wired; mapping is hard-coded. |

---

## 3. Target architecture

Introduce a clear split between **per-session** state and **process-lifetime**
state, because the replay deliberately tears down and rebuilds the session.

```
ReplaySessionContext   (process-lifetime singleton, NOT an IGameSystem)
  ├─ selected mission id + chosen options (facts preset, build mode, loadout)
  ├─ captured source state (serialized; see section 4A):
  │     ├─ character customization state (already captured)
  │     ├─ player progression snapshot (level/attrs/perks/cyberware)   [new]
  │     └─ inventory snapshot                                          [new]
  ├─ return target:
  │     └─ PONR id / origin save name                                 [new]
  └─ phase flag: Idle | CapturingSource | LoadingReplay | InReplay | Returning

ReplayManager : IGameSystem   (per-session; re-created on each session)
  └─ on init, re-binds to the live systems and reads ReplaySessionContext
     to decide what to apply this session (init facts, build, inventory).
```

Rationale: `StartReplayGameDefinition` currently stores the selected def +
customization on the manager and immediately starts a new session. After the
session swap the manager instance is gone, so the *next* session's manager must
read intent from a place that survived. A standalone context object owned by the
plugin (or a small serialized blob) is that place. **[verify in-game]** whether
`IGameSystem` instances persist across `StartSession`; if they do persist, the
context can stay on the manager, but the safer assumption is that they do not.

---

## 4. Phased plan

### Phase 0 — Foundations & verification (do first)

0.1 **Confirm game-system lifetime across `StartSession`.** Add temporary
    logging in `OnInitialize`/`OnUninitialize`/`OnWorldDetached` and observe
    whether `ReplayManager` is destroyed/recreated on a replay launch. This
    decides whether section 3's split is mandatory. **[verify in-game]**

0.2 **Confirm the telemetry PONR offsets** (`184` → `0xD8`) against a current
    build via RTTI dump / NativeDB / decompiled `TelemetrySystem`. Wrap the raw
    offset reads with `RED4EXT_ASSERT_*` once confirmed. **[verify in-game]**

0.3 **Decide the comms mechanism** (Phase 5). Keep the FactsDBManager hook for
    now; spike a dynamic quest node in parallel.

### Phase 1 — Mission catalog + entry UI

Goal: let the player actually pick and start a replay.

1.1 Replace the hard-coded `EReplayGameDefinition` switch
    (`ReplayManager.cpp:196`) with a **data-driven mission catalog**: id →
    `{ gamedef path, display name, default facts preset, build mode, loadout
    id }`. A simple native table is fine initially; consider TweakDB records or
    a JSON resource later so missions can be added without recompiling.

1.2 Wire all existing gamedefs (q113, q115, q306, boss_rush, smasher) into the
    catalog.

1.3 **REDscript layer**: declare `native class ReplayManager` /
    `ReplayFactDefinition` mirrors and a `GameInstance.GetReplayManager()`-style
    accessor (RedLib game-system accessor). Add a thin menu — start with the
    pause/main menu, hooking an existing menu controller via `@wrapMethod` and
    injecting a "Mission Replay" entry, or a hotkey-driven debug list first to
    de-risk the flow before investing in ink screens.

1.4 The menu calls `SetQuestState(presetFacts)` then
    `StartReplayGameDefinition(missionId)`.

### Phase 2 — Source-session capture & cross-session carry-over

Goal: **preserve player state** across the clean-slate reload. Player state is
preserved if at all possible; curated per-mission loadouts are only a fallback
for content where the captured state is unusable. Serialization strategy is
detailed in **section 4A**.

2.1 Build `ReplaySessionContext` (section 3) as a plugin-owned singleton.

2.2 At launch time (still in the live session, inside
    `StartReplayGameDefinition` *before* `LoadGameDefinitionByPath`):
    - capture character customization (already done — move into context);
    - **capture the PONR / origin save** so we can return (Phase 4);
    - **read player progression values** out of `PlayerDevelopmentSystem` /
      `PlayerDevelopmentData` (level, attributes, perks, proficiencies,
      cyberware) — extract the discrete values, do **not** snapshot the system
      structure (section 4A). `SetProgressionBuildRequest` stays a lossy
      fallback only.
    - **read inventory/equipment** — enumerate items + slotting via
      `TransactionSystem.GetItemList` and `EquipmentSystem` (section 4A) to
      reproduce later; curated loadout only as fallback.

2.3 Persist the context. If 0.1 shows the manager survives, an in-memory blob is
    enough. If not, write the serialized blob to a file under the plugin folder
    (or RED4ext persistent storage) keyed simply as "pending replay" — only one
    replay is in flight at a time.

### Phase 2b — Preset save/load

Goal: let players save a captured state + options set as a **named preset** and
reload it later (also useful for shipping curated presets and for testing).

2b.1 A preset = the same serialized payload as the carry-over blob (player
     progression + inventory + customization) plus the chosen facts/options,
     written to a named file under the plugin folder.

2b.2 Expose `SaveReplayPreset(name)` / `LoadReplayPreset(name)` /
     `ListReplayPresets()` to RTTI so the UI (Phase 1) can drive them. Loading a
     preset populates `ReplaySessionContext` exactly as a live capture would, so
     the downstream apply path (Phase 3) is identical whether state came from a
     live capture or a preset.

2b.3 Version the preset format (see section 4A) so old presets fail gracefully
     after a game/mod update instead of corrupting a session.

### Phase 3 — Apply captured state in the replay session

Goal: make `SetupQuestState` / `SetupPlayerData` / `SetupInventory` real.

3.1 On `ReplayStarted` (`ReplayManager.cpp:68`), read `ReplaySessionContext`:
    - `SetupQuestState`: apply the mission's **facts preset** (header comment's
      Takemura-alive / Oda-alive / Jackie-told flags) merged with any
      player-chosen overrides. Centralize preset definitions in the catalog.
    - `SetupPlayerData`: default → re-apply the captured progression *values* by
      driving PDS through its own requests/setters (section 4A); the chosen
      static progression build stays behind the `UseStaticProgressionBuild`
      toggle as a debug fallback.
    - `SetupInventory`: default → re-grant the captured items via the
      transaction/inventory system and re-equip; curated mission loadout only as
      fallback.

3.2 Keep the existing handshake: after setup,
    `FactsDB.SetFact("replay_init_finished", 1)` to release the quest gate.
    Make sure setup is idempotent and null-safe (handles can be invalid early
    in session load — the code already early-returns on missing systems; keep
    that discipline).

### Phase 4 — Return / PONR round-trip

Goal: when the replay finishes, send the player back where they were.

4.1 Implement the capture: call `CapturePointOfNoReturnId` at launch and store
    the result (origin save name and/or PONR id) in `ReplaySessionContext`,
    not on the per-session manager (`ReplayManager.cpp:119`).

4.2 On `ReplayEnded` (`ReplayManager.cpp:85`): instead of a bare `ExitToMenu`,
    resolve the stored return target and call
    `SessionLoader::LoadSavedGameByName(originSave)` so the player resumes their
    real save. Fall back to `ExitToMenu` if no return target is set (e.g. replay
    launched from the main menu rather than from a live game).
    Resolve the open question in section 6 on whether we make a fresh save at
    launch vs. just remember the latest existing save name.

4.3 Provide an explicit "abandon replay" path (menu/hotkey) that also routes
    through `ReplayEnded` so the return logic is shared.

### Phase 5 — Comms hardening

5.1 Evaluate dynamic quest nodes vs. the `FactsDBManager::Execute` hook
    (`ReplayComms.cpp:13`). If a dynamic node can carry a typed command +
    payload cleanly, migrate to it and drop the string-prefix convention; it is
    less brittle than overloading fact names and avoids sharing the hook with
    `DynamicSpawnSystemNodeDefinition` / `RewardManagerNodeDefinition`
    (`ReplayComms.cpp:32`). Otherwise keep the hook but document the fact-name
    contract centrally.

5.2 Formalize the command vocabulary in one header so quest authors and native
    code share a single source of truth (`INITIALIZE_ALL`, `FINISHED`, plus any
    future per-phase signals like "checkpoint reached").

### Phase 6 — Quest authoring conventions & content

6.1 Document the **per-mission quest-tree authoring contract** (see section 5)
    so new replays are mechanical to add.

6.2 Finish wiring + smoke-test each existing mission tree end to end:
    `INITIALIZE_ALL` at entry, `FINISHED` at all exits, correct spawn tag,
    correct world, gamedef points at the right quest.

6.3 Re-enable and author the custom replay loading screen
    (`SessionLoader.cpp` commented `m_loadingScreen`).

---

## 4A. Player-state preservation & preset serialization

Preservation is the default; this is the strategy for capturing player state in
the live session and rehydrating it in the clean-slate replay session (and to/from
preset files). Study [`psiberx/cp2077-codeware`](https://github.com/psiberx/cp2077-codeware)
as the reference for natively serializing/deserializing game types.

### What codeware actually gives us

Codeware does **not** expose a single "serialize any game type to a blob" call.
What it does provide, and what we lean on:

- **Reflection** (`Reflection`, `ReflectionClass/Type/Prop/Func/Enum` under
  `src/App/Reflection`) for reading specific values off a live instance when
  there is no convenient native getter — this is our main use, since we extract
  values rather than serialize whole systems (see below).
- **Engine-backed persistence of `persistent` fields.** A codeware
  `ScriptableService` (or scriptable system) can mark fields `persistent`, and
  the game's own persistency system serializes/deserializes them across saves.
  We use this to *store our extracted value set* in a persistent container, not
  to serialize game systems wholesale. **[verify in source]** how codeware
  registers the service so the engine includes it in the persistency set.
- **Variant marshaling** (`FromVariant<T>`, `VariantTypeName`) for moving typed
  values across the native/script boundary without bespoke glue.

> The repo isn't checked out here, so treat the file/symbol names above as
> [verify in source]; fetch it (see section 9) and read `src/App/Reflection` and
> its persistency/service handling before implementing.

### Capture/restore approach

**Guiding principle: extract the values we need and re-apply them through the
owning system's own API — do not replace/overwrite the system structure itself.**
The session creates and wires its systems (`PlayerDevelopmentSystem`, inventory,
etc.); dropping a deserialized instance over the top risks half-initialized
systems and broken internal links. So we read discrete state out of the live
session and *reproduce* it in the replay session via the normal mutation paths.

1. **Player progression — read values from PDS, re-apply via PDS (primary).**
   Read the discrete values from `PlayerDevelopmentSystem` in the live session
   and re-apply them in the replay session by queuing PDS's own
   `PlayerScriptableSystemRequest`s, so the system stays internally consistent —
   never by overwriting the PDS instance. The getter→request surface below is
   **verified against the decompiled scripts** (`cyberpunk/systems/
   playerDevelopmentSystem.swift` + `playerDevelopmentSystemRequests.swift`);
   the only remaining in-game unknowns are apply *ordering/threading*, not the
   API shape.

   | State | Read (getter) | Re-apply (queued request) |
   |-------|---------------|---------------------------|
   | Attributes | `GetAttributes() -> [SAttribute]` (`SAttribute{ attributeName: gamedataStatType, value: Int32 }`) | `SetAttribute.Set(owner, level: Float, type: gamedataStatType)` |
   | New perks (2.x) | per `gamedataNewPerkType`: `GetPerkLevel(newPerkType) -> Int32` / `IsNewPerkBought` | `BuyNewPerk(perkType, forceBuy: true)` once per level — `forceBuy` bypasses prereqs so ordering doesn't bite |
   | Proficiency levels | `GetProficiencyLevel(type: gamedataProficiencyType)` (+ `GetTotalProfExperience`) | `SetProficiencyLevel.Set(owner, level, type, telemetryGainReason)` |
   | Traits | `GetTraitLevel(traitType)` | `IncreaseTraitLevel.Set(owner, traitType)` per level |
   | Unspent dev points | `GetDevPoints(type: gamedataDevelopmentPointType)` | `AddDevelopmentPoints.Set(owner, amount, type)` |

   Notes: use `forceBuy: true` on `BuyNewPerk` and re-apply **attributes before
   perks** to sidestep prerequisite/point gating. There are legacy "old perk"
   APIs (`GetPerks() -> [SPerk]`, `gamedataPerkType`); modern saves use the
   `NewPerk` path — capture both only if old-perk content is in scope. The
   existing `SetProgressionBuild` request remains the lossy fallback (it applies
   a whole TweakDB build record, not the player's exact state). Use codeware
   **Reflection** only for any field lacking a native getter.
2. **Inventory/equipment — reproduce items, don't transplant containers.**
   Enumerate the player's items + slotting, then re-grant and re-equip through
   the native `TransactionSystem` / `EquipmentSystem` in the replay session
   rather than swapping container objects. Surface **verified against decompiled
   scripts** (`TransactionSystem` in `orphans.swift`, `EquipmentSystem` in
   `cyberpunk/systems/equipmentSystem.swift`). Owner = player `GameObject` from
   `PlayerSystem`.

   *Enumerate (capture):*
   - All items: `TransactionSystem.GetItemList(player, out [wref<gameItemData>])`
     (tag-filtered variants exist: `GetItemListByTags`, `…ExcludingTags`).
   - Per `gameItemData`: `GetID() -> ItemID`, `GetQuantity() -> Int32`,
     `GetItemType() -> gamedataItemType`, `HasTag()`, `GetStatValueByType()`.
     Resolve the backing record with `ItemID.GetTDBID(itemID) -> TweakDBID`.
   - **Quality** (required for items that don't derive it from their record and
     can't be upgraded — e.g. cyberware): capture
     `RPGManager.GetItemDataQuality(itemData) -> gamedataQuality`. Without this,
     a re-granted cyberware/quality-bearing item rolls or defaults to the wrong
     quality. (Quality also exists as a stat via
     `GetStatValueByType(gamedataStatType.Quality)` ↔
     `RPGManager.ItemQualityEnumToValue`.)
   - **RNG seed** (to reproduce exact item identity + rolled stats): capture
     `ItemID.GetRngSeed(itemID) -> Uint32` for items whose stats are
     seed-derived.
   - Equipped state: `EquipmentSystem.GetPlayerData(player) ->
     EquipmentSystemPlayerData`, then walk areas/slots via
     `GetItemInEquipSlot(areaType, slotIndex) -> ItemID`, `IsEquipped(itemID)`,
     `GetSlotIndex(itemID)`. Capture cyberware/clothing/weapon slots.

   *Reproduce (restore):*
   - **Rebuild the exact `ItemID`** from the captured TDBID + seed with
     `ItemID.CreateFromSeedWithOffset(newItemTDBID, seed, offset)` (the `offset`
     is the unique-counter component; `DuplicateRandomSeedWithOffset` is the
     same idea relative to a reference item). Reproducing the seed gives the same
     rolled stats and a stable identity, so we don't have to settle for "same
     record, different roll."
   - Re-grant in bulk: `TransactionSystem.GiveItems(player, [ItemModParams])`,
     where `ItemModParams { itemID: ItemID, quantity: Int32, customPartsToInstall:
     [ItemID] }` carries the rebuilt `ItemID` + mods/attachments. Simple stacks:
     `GiveItem(player, itemID, amount)`; clone a captured item's data with
     `GiveItemByItemData(player, gameItemData)`.
   - **Force captured quality** on items that need it (cyberware, anything
     non-upgradeable that doesn't get quality from its record): apply the
     captured `gamedataQuality` — set the Quality stat, or use
     `EquipmentSystem.ForceQualityAndDuplicateStatsShard(owner, originalItemID,
     destinationItemID)` where applicable. **[verify in-game]** the cleanest
     forcing path.
   - Re-equip: `EquipmentSystemPlayerData.EquipItem(itemID, …)` per captured
     slot; for explicit slot placement (cyberware/garment) use
     `TransactionSystem.AddItemToSlot(player, slotID, itemID, …)`.

   *Caveats (the remaining open work):* anchor each captured slot on **record
   TDBID + seed + quality** so identity survives the session swap (seed rebuilds
   via `CreateFromSeedWithOffset`; quality is forced explicitly). Quest-tagged
   items and items whose records don't exist in the clean-slate session may
   refuse to grant; skip + warn (section 6 #2). Crafted/upgraded stats beyond
   `customPartsToInstall` + seed may need extra stat re-application
   **[verify in-game]**.
3. **Curated fallback (last resort).** `SetProgressionBuildRequest`
   (`ReplayManager.cpp:159`) and per-mission item loadouts — lossy, only when the
   real values can't be reproduced (e.g. an item record missing in the
   clean-slate session).

This makes the persistence payload a set of plain, versionable values rather than
opaque engine bytes, which also keeps presets (Phase 2b) stable and inspectable.

### Blob/preset format

- Wrap every serialized payload with a small header: `magic`, `formatVersion`,
  `gameVersion`, and a section table (customization / progression / inventory /
  options-facts). The carry-over blob and the preset file share this format; a
  preset is just a named, on-disk carry-over blob plus the chosen options.
- **Version-gate on load.** Reject or migrate on `formatVersion` mismatch and
  warn on `gameVersion` mismatch rather than re-applying stale values (e.g. a
  perk/item id removed by an update) into a live session (see Risks).
- Keep all serialization on the request/`Tick` path, not on the quest hook
  thread, and validate handles at apply time (handles are unstable early in
  session load).

## 5. Quest-tree authoring contract (for WolvenKit content)

Each replayable mission must:

1. Have a `*.gamedef` whose `mainQuests[0].questFile` points at the mission's
   custom `*.quest`, with `world` = Night City and a valid `spawnPointTags`
   entry (see `replay_q113.gamedef` → `#q113_ws_player_estate_spwn`).
2. Begin its quest graph with a node that sets the FactsDB var
   `REPLAY_CALL_HOOK_INITIALIZE_ALL` (exact value `1`, `setExactValue`), then
   **waits on** `replay_init_finished == 1` before proceeding. This is the
   native init handshake (`ReplayComms.cpp:53`, `ReplayManager.cpp:82`).
3. End every terminal path by setting `REPLAY_CALL_HOOK_FINISHED`
   (`ReplayComms.cpp:54`).
4. Contain only the phases relevant to that mission (clean-slate principle); do
   not depend on upstream quest state that the replay session will not have.
5. Register all new resource paths (gamedef/quest/phase) in
   `wolvenkit/custom_refs.txt`.

> Authoring caveat: the hook recognizes the command only when the `SetVar` node
> uses `setExactValue` with a non-zero value (`ReplayComms.cpp:37`). Document
> this so authors don't use increment/toggle nodes for the signal.

---

## 6. Open questions / decisions needed

1. **Build preservation.** *Decided: preserve the player's real progression by
   default* by extracting discrete values from PDS and re-applying them through
   PDS's own API — **not** by serializing/replacing the system structure
   (section 4A). Static progression builds kept only as a debug/fallback option.
   The getter/request surface is settled (section 4A table, verified against
   decompiled scripts); remaining open sub-question is apply *ordering/timing* in
   the freshly-loaded session. **[verify in-game]**
2. **Inventory preservation.** *Decided: preserve the player's real inventory by
   default* by enumerating items and re-granting them through the
   inventory/transaction API (section 4A); curated per-mission loadouts are a
   fallback for content where the captured inventory is unusable. The
   enumerate/grant/equip surface is settled (section 4A, verified against
   decompiled scripts), including exact-identity reproduction (TDBID + RNG seed
   via `CreateFromSeedWithOffset`) and quality capture for cyberware. Remaining
   open work: the cleanest quality-forcing path and handling items whose
   records/quest tags don't exist in the clean-slate session (skip + warn).
3. **Return target.** Make a dedicated "pre-replay" save at launch (clean, but
   adds a save file and write cost) vs. remember the most recent existing save
   and reload it (no extra file, but the player may have been mid-mission).
   Recommendation: capture the PONR id where available; otherwise create a
   single reusable pre-replay autosave slot.
4. **Game-system lifetime** (Phase 0.1) decides whether `ReplaySessionContext`
   can be in-memory or must be serialized.
5. **Composite missions** (boss_rush) — how options/facts are presented when a
   "mission" is actually several chained phases.

---

## 7. Risks

- **Unverified offsets** (telemetry PONR, char-customization `0x78`) will break
  on game updates; guard with `RED4EXT_ASSERT_*` and prefer RTTI/named access
  where a script-facing API exists. **[verify in-game]**
- **Session-swap timing**: applying player/inventory/quest state too early in
  session load can hit invalid handles. The existing early-return guards are the
  right pattern; keep setup on the `PostBuckets` tick and re-check validity.
- **Threading**: quest callbacks run off the main thread; continue funnelling
  through `AddRequest`/`Tick` and the existing spinlocks — do not touch live
  game systems directly from the hook.
- **Clean-slate assumptions leaking**: a quest tree that quietly relies on
  prior state will fail only at runtime in the replay session; the authoring
  contract (section 5) is the mitigation.
- **Stale state across updates**: captured value sets and saved presets can
  reference perk/item/record ids that a game or mod update removed or changed.
  Mitigate with the versioned header + load-time gating in section 4A; skip and
  warn on unknown ids rather than aborting the whole apply.
- **Overwriting session-owned systems**: the reason we extract values and
  re-apply via each system's API (section 4A) instead of transplanting
  serialized system instances — dropping a deserialized system over one the
  session already created risks half-initialized state and broken internal
  links. Keep new code on the value-extraction path; validate handles at apply
  time.

---

## 8. Suggested immediate next steps

1. Phase 0.1 + 0.2 logging spikes to settle lifetime/offset assumptions.
2. Phase 1.1–1.2: data-driven catalog wiring all five existing gamedefs.
3. Phase 1.3 minimal debug entry (hotkey list) → prove a full launch→init→play
   →finish→return loop on **one** mission (q113) before building real UI.
4. Phase 4 PONR round-trip on that same mission so the loop is closed end to end.

Once the single-mission loop is closed and verified in-game, fan out the
remaining missions and the richer capture (build + inventory) and UI work.

---

## 9. Reference map (where to look)

- Native conventions / RTTI / game systems: cp2077-modding skill
  `references/native.md`.
- REDscript hooks / decompiled scripts as behavior ground truth:
  `references/redscript.md`. Use decompiled `PlayerDevelopmentSystem`,
  `TelemetrySystem`, `gameuiCharacterCustomizationSystem`, and the gameplay
  session / ink `SystemRequestsHandler` for [verify in-game] items.
- Native serialization / persistence / reflection reference:
  [`psiberx/cp2077-codeware`](https://github.com/psiberx/cp2077-codeware) —
  study `src/App/Reflection`, its `ScriptableService` `persistent`-field
  handling, and Variant marshaling. Fetch it into the source cache (do not
  vendor) the same way as the other upstreams.
- Address-hash workflow for any offsets that get promoted to reloc functions:
  cp2077-address-hash skill.
- Project code anchors: `src/Manager/ReplayManager.cpp`,
  `src/Session/SessionLoader.cpp`, `src/Comms/ReplayComms.cpp`,
  `wolvenkit/source/archive/mod/quest/replay/`.
