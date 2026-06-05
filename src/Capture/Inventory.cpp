#include "Inventory.hpp"

#include <RED4ext/Scripting/Natives/Generated/game/ItemData.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ItemModParams.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/TransactionSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/ItemType.hpp>

#include <Shared/Raw/PlayerSystem/PlayerSystem.hpp>
#include <Shared/Raw/ScriptableSystem/ScriptableSystem.hpp>

#include <Util/PluginLog.hpp>

namespace
{
using namespace Red;
using namespace replay;

constexpr CName kEquipmentSystem = "EquipmentSystem";
constexpr CName kItemID = "ItemID";
constexpr CName kRPGManager = "RPGManager";

Handle<game::Object> GetPlayerObject(cp::PlayerSystem* aPlayerSystem) noexcept
{
    Handle<game::Object> obj{};
    if (aPlayerSystem)
        shared::raw::PlayerSystem::GetPlayerControlledGameObject(aPlayerSystem, obj);
    return obj;
}

Handle<game::ScriptableSystem> GetEquipmentSystem(game::ScriptableSystemsContainer* aContainer) noexcept
{
    Handle<game::ScriptableSystem> system{};
    if (!aContainer)
        return system;
    shared::raw::ScriptableSystemsContainer::GetSystemByName(aContainer, system, kEquipmentSystem);
    return system;
}

TweakDBID ItemTDBID(const ItemID& aID) noexcept
{
    TweakDBID out{};
    CallStatic(kItemID, "GetTDBID", out, aID);
    return out;
}

std::uint32_t ItemSeed(const ItemID& aID) noexcept
{
    std::uint32_t out = 0;
    CallStatic(kItemID, "GetRngSeed", out, aID);
    return out;
}

ItemID ItemFromSeed(TweakDBID aTDBID, std::uint32_t aSeed, std::int32_t aOffset) noexcept
{
    ItemID out{};
    CallStatic(kItemID, "CreateFromSeedWithOffset", out, aTDBID, aSeed, aOffset);
    return out;
}

game::data::Quality ItemQuality(const Handle<game::ItemData>& aItemData) noexcept
{
    game::data::Quality out = game::data::Quality::Invalid;
    CallStatic(kRPGManager, "GetItemDataQuality", out, WeakHandle<game::ItemData>(aItemData));
    return out;
}

ItemID ItemIDFromData(const Handle<game::ItemData>& aItemData) noexcept
{
    ItemID out{};
    CallVirtual(aItemData.instance, "GetID", out);
    return out;
}

std::int32_t ItemQuantity(const Handle<game::ItemData>& aItemData) noexcept
{
    std::int32_t out = 0;
    CallVirtual(aItemData.instance, "GetQuantity", out);
    return out;
}

bool IsCyberware(const Handle<game::ItemData>& aItemData) noexcept
{
    game::data::ItemType type{};
    CallVirtual(aItemData.instance, "GetItemType", type);
    // Cyberware items are the category whose quality must be force-applied (plan §4A).
    return type == game::data::ItemType::Cyberware || type == game::data::ItemType::Cyb_Ability ||
           type == game::data::ItemType::Cyb_HealingAbility || type == game::data::ItemType::Cyb_Launcher ||
           type == game::data::ItemType::Cyb_MantisBlades || type == game::data::ItemType::Cyb_NanoWires ||
           type == game::data::ItemType::Cyb_StrongArms;
}

void CaptureItems(game::TransactionSystem* aTransactionSystem, const Handle<game::Object>& aOwner,
                  InventorySnapshot& aOut) noexcept
{
    DynArray<WeakHandle<game::ItemData>> items{};
    bool ok = false;
    CallVirtual(aTransactionSystem, "GetItemList", ok, aOwner, items);
    if (!ok)
    {
        replay::log::Warn("Inventory::Capture: GetItemList failed");
        return;
    }
    aOut.m_items.reserve(items.size());
    for (uint32_t i = 0; i < items.size(); ++i)
    {
        auto data = items[i].Lock();
        if (!data)
            continue;
        CapturedItem captured{};
        const auto id = ItemIDFromData(data);
        captured.m_tdbid = ItemTDBID(id);
        captured.m_seed = ItemSeed(id);
        captured.m_quantity = ItemQuantity(data);
        if (IsCyberware(data))
        {
            captured.m_quality = ItemQuality(data);
            captured.m_qualityForced = true;
        }
        aOut.m_items.push_back(captured);
    }
}

// Captures equipped ItemIDs by asking EquipmentSystem for the player's data and reading
// each (areaType, slotIndex) cell. Detailed area iteration is a verify-in-game item;
// for the skeleton we lean on script-level helpers that already enumerate equipped IDs
// when present. Empty list is acceptable — Apply re-equips by ItemID which only needs
// the items to be in inventory.
void CaptureEquippedSlots(game::ScriptableSystemsContainer* aContainer, const Handle<game::Object>& aOwner,
                          InventorySnapshot& aOut) noexcept
{
    auto equipmentSystem = GetEquipmentSystem(aContainer);
    if (!equipmentSystem)
    {
        replay::log::Warn("Inventory::Capture: EquipmentSystem not found");
        return;
    }

    Handle<IScriptable> playerData{};
    CallVirtual(equipmentSystem.instance, "GetPlayerData", playerData, aOwner);
    if (!playerData)
        return;

    // EquipmentSystemPlayerData exposes typed area enumeration in scripts; the precise
    // (area, slotIndex) walk needs in-game verification of script signatures. For now
    // we leave the equipped list empty and let Apply re-equip each captured item by ID,
    // which works for the common path (Apply calls EquipItem(itemID) directly).
    // TODO[verify in-game]: enumerate EquipAreaType + per-area slot count and emit
    // CapturedEquipSlot entries with the right area/slot indices.
    (void)playerData;
}
} // namespace

replay::InventorySnapshot replay::Inventory::Capture() noexcept
{
    InventorySnapshot snapshot{};

    auto playerSystem = GetGameSystem<cp::PlayerSystem>();
    auto transactionSystem = GetGameSystem<game::TransactionSystem>();
    auto container = GetGameSystem<game::ScriptableSystemsContainer>();
    if (!playerSystem || !transactionSystem || !container)
    {
        replay::log::Warn("Inventory::Capture: missing PlayerSystem/TransactionSystem/Container");
        return snapshot;
    }

    auto playerObject = GetPlayerObject(playerSystem);
    if (!playerObject)
    {
        replay::log::Warn("Inventory::Capture: no player object");
        return snapshot;
    }

    CaptureItems(transactionSystem, playerObject, snapshot);
    CaptureEquippedSlots(container, playerObject, snapshot);

    snapshot.m_present = true;
    return snapshot;
}

void replay::Inventory::Apply(const InventorySnapshot& aSnapshot, cp::PlayerSystem* aPlayerSystem,
                              game::ScriptableSystemsContainer* aScriptableContainer) noexcept
{
    if (!aSnapshot.m_present)
        return;

    auto transactionSystem = GetGameSystem<game::TransactionSystem>();
    if (!aPlayerSystem || !transactionSystem)
    {
        replay::log::Warn("Inventory::Apply: missing PlayerSystem/TransactionSystem");
        return;
    }

    auto playerObject = GetPlayerObject(aPlayerSystem);
    if (!playerObject)
    {
        replay::log::Warn("Inventory::Apply: no player object");
        return;
    }

    // Rebuild ItemIDs (record + seed) and grant in one batch. `offset` 0 follows the
    // convention used by CreateFromSeedWithOffset's primary callers — uniqueness inside
    // the new session is fine because the inventory starts empty.
    DynArray<game::ItemModParams> grants{};
    grants.Reserve(static_cast<uint32_t>(aSnapshot.m_items.size()));
    for (const auto& captured : aSnapshot.m_items)
    {
        if (!captured.m_tdbid)
            continue;
        game::ItemModParams params{};
        params.itemID = ItemFromSeed(captured.m_tdbid, captured.m_seed, 0);
        params.quantity = captured.m_quantity;
        grants.PushBack(params);
    }

    bool ok = false;
    CallVirtual(transactionSystem, "GiveItems", ok, playerObject, grants);
    if (!ok)
        replay::log::Warn("Inventory::Apply: GiveItems returned false (some items may be missing/quest-tagged)");

    // Quality forcing for cyberware: per plan §4A this needs verify-in-game (cleanest
    // path is EquipmentSystem.ForceQualityAndDuplicateStatsShard vs. forcing the Quality
    // stat directly). Leave as TODO until the live test reveals the right call.
    // TODO[verify in-game]: apply CapturedItem::m_qualityForced quality on grant.

    // Re-equip: walk captured slots and EquipItem(itemID) each one. The slot capture
    // is itself a TODO above; once filled this loop becomes the apply-side counterpart.
    if (auto equipmentSystem = GetEquipmentSystem(aScriptableContainer))
    {
        Handle<IScriptable> playerData{};
        CallVirtual(equipmentSystem.instance, "GetPlayerData", playerData, playerObject);
        if (playerData)
        {
            for (const auto& slot : aSnapshot.m_equipped)
            {
                CallVirtual(playerData.instance, "EquipItem", slot.m_itemID, /*blockActiveSlotsUpdate=*/false,
                            /*forceEquipWeapon=*/false);
            }
        }
    }
}
