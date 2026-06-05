#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <Session/ReplaySessionContext.hpp>

using namespace Red;

namespace replay::Inventory
{
// Enumerate items + equipped slots via TransactionSystem / EquipmentSystem (plan §4A,
// surface verified against decompiled scripts). Returns a non-present snapshot if the
// player or systems are unavailable.
InventorySnapshot Capture() noexcept;

// Re-grant captured items in the clean-slate session and re-equip. Items whose record
// is missing in the new session are skipped + logged (see plan §6 #2). Caller must
// hold a valid PlayerSystem.
void Apply(const InventorySnapshot& aSnapshot, cp::PlayerSystem* aPlayerSystem,
           game::ScriptableSystemsContainer* aScriptableContainer) noexcept;
} // namespace replay::Inventory
