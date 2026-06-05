#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/DevelopmentPointType.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/NewPerkType.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/ProficiencyType.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/Quality.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/StatType.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/TraitType.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationState.hpp>
#include <RedLib.hpp>

#include <Manager/MissionCatalog.hpp>

#include <string>
#include <vector>

using namespace Red;

namespace replay
{
enum class EReplayPhase
{
    Idle,             // No replay in flight.
    CapturingSource,  // Snapshotting live-session state, gamedef not yet kicked off.
    LoadingReplay,    // LoadGameDefinitionByPath issued, waiting for session swap.
    InReplay,         // ReplayManager has seen ReplayStarted and applied state.
    Returning,        // ReplayEnded fired, returning to origin save / menu.
};

struct CapturedAttribute
{
    game::data::StatType m_type{};
    std::int32_t m_value{};
};

struct CapturedNewPerk
{
    game::data::NewPerkType m_type{};
    std::int32_t m_level{};
};

struct CapturedProficiency
{
    game::data::ProficiencyType m_type{};
    std::int32_t m_level{};
};

struct CapturedTrait
{
    game::data::TraitType m_type{};
    std::int32_t m_level{};
};

struct CapturedDevPoints
{
    game::data::DevelopmentPointType m_type{};
    std::int32_t m_unspent{};
};

struct PlayerProgressionSnapshot
{
    std::vector<CapturedAttribute> m_attributes;
    std::vector<CapturedNewPerk> m_newPerks;
    std::vector<CapturedProficiency> m_proficiencies;
    std::vector<CapturedTrait> m_traits;
    std::vector<CapturedDevPoints> m_devPoints;

    bool m_present{};
};

// Anchor an item by record + RNG seed + quality so identity survives the session swap
// (see plan §4A). `m_quality` is only meaningful for items the apply-side must force
// (cyberware, etc.); leave it as the SDK default for items that derive it from records.
struct CapturedItem
{
    TweakDBID m_tdbid{};
    std::uint32_t m_seed{};
    std::int32_t m_quantity{};
    game::data::Quality m_quality{};
    bool m_qualityForced{}; // set when we captured quality off the live item
};

struct CapturedEquipSlot
{
    // EquipmentSystemPlayerData walks slot arrays by (area, slotIndex). We re-issue
    // EquipItem(itemID) on apply, so capturing the slot mostly helps diagnostics and
    // an explicit AddItemToSlot fallback for cyberware.
    std::int32_t m_areaType{};
    std::int32_t m_slotIndex{};
    ItemID m_itemID{};
};

struct InventorySnapshot
{
    std::vector<CapturedItem> m_items;
    std::vector<CapturedEquipSlot> m_equipped;

    bool m_present{};
};

struct ReturnTarget
{
    // Captured at launch time. PONR id is the preferred resume anchor; m_originSaveName
    // is the fallback (most-recent existing save). One or both may be empty.
    std::string m_pointOfNoReturnId;
    std::string m_originSaveName;

    bool HasAny() const noexcept { return !m_pointOfNoReturnId.empty() || !m_originSaveName.empty(); }
};

// Plugin-owned, process-lifetime singleton. The captured state lives here so it survives
// the new-session swap regardless of game-system lifetime assumptions (the per-session
// ReplayManager re-reads it after ReplayStarted). Access is implicitly serialized by the
// phase enum — capture happens in CapturingSource (live session), apply in InReplay
// (clean-slate session), and the RTTI surface coordinates phase transitions.
class ReplaySessionContext
{
public:
    static ReplaySessionContext& Get() noexcept;

    void Reset() noexcept;

    EReplayPhase m_phase{EReplayPhase::Idle};
    const MissionCatalogEntry* m_mission{};

    Handle<game::ui::CharacterCustomizationState> m_characterCustomization{};
    PlayerProgressionSnapshot m_progression{};
    InventorySnapshot m_inventory{};
    ReturnTarget m_returnTarget{};

    // Caller-staged facts merged with the mission's default preset on apply.
    std::vector<ReplayFactPreset> m_stagedFacts{};

private:
    ReplaySessionContext() = default;
};
} // namespace replay
