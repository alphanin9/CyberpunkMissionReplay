#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <Session/ReplaySessionContext.hpp>

using namespace Red;

namespace replay::PlayerProgression
{
// Reads attributes/perks/proficiencies/traits/dev-points from the live PDS via reflection
// (no SDK getters generated for these). Returns an empty / `!m_present` snapshot on
// missing/invalid context. Safe to call from the live session thread before launching.
PlayerProgressionSnapshot Capture() noexcept;

// Re-applies a previously captured snapshot in the clean-slate replay session by
// queueing PDS's own PlayerScriptableSystemRequests. Attributes first, then perks
// (forceBuy = true), then proficiencies / traits / dev-points (see plan §4A).
// Caller must hold valid PlayerSystem + ScriptableSystemsContainer.
void Apply(const PlayerProgressionSnapshot& aSnapshot, cp::PlayerSystem* aPlayerSystem,
           game::ScriptableSystemsContainer* aScriptableContainer) noexcept;
} // namespace replay::PlayerProgression
