#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/cp/PlayerSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ScriptableSystemsContainer.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/QuestsSystem.hpp>
#include <RedLib.hpp>

#include <Shared/Raw/Ink/InkSystem.hpp>

using namespace Red;

namespace replay
{
enum class EReplayRequestType
{
    ReplayStarted,
    ReplayEnded
};

// Set up quest state via facts. Used for staging both per-mission default presets and
// player-chosen overrides (Takemura alive / Oda alive / Jackie told, etc.).
class ReplayFactDefinition : public IScriptable
{
public:
    CString m_factName{};
    int m_factValue{};

    void SetFactName(CString aName) { m_factName = aName; }
    void SetFactValue(int aValue) { m_factValue = aValue; }

    RTTI_IMPL_TYPEINFO(ReplayFactDefinition);
    RTTI_IMPL_ALLOCATOR();
};

class ReplayManager : public IGameSystem
{
    // Player progression / inventory default to live-state capture; this toggle keeps
    // the legacy static-build path available as a debug fallback (plan §3.1).
    static constexpr bool kUseStaticProgressionBuild = false;
    static constexpr TweakDBID kDebugProgressionBuildTDBID = "ProgressionBuilds.VHard_50_RefBody";

    inline static ReplayManager* s_this{};

    DynArray<EReplayRequestType> m_replayRequests{};
    SharedSpinLock m_replayRequestLock{};

    cp::PlayerSystem* m_playerSystem{};
    game::ScriptableSystemsContainer* m_scriptableSystemsContainer{};
    quest::QuestsSystem* m_questsSystem{};
    shared::raw::Ink::InkSystem* m_inkSystem{};

    // Setup correct quest state based on staged facts + mission preset.
    void SetupQuestState() noexcept;

    // Apply captured player progression (preferred) or the debug build (fallback).
    void SetupPlayerData() noexcept;

    // Re-grant captured inventory and re-equip.
    void SetupInventory() noexcept;

    // Capture the current telemetry PONR id and the origin save name into the session
    // context so ReplayEnded can resume the player's prior save.
    void CaptureReturnTarget() noexcept;

    // Resume the saved game named in the session context, or fall back to ExitToMenu.
    void ResumeOrExit() noexcept;

    void Tick(JobQueue& aQueue) noexcept;

public:
    void OnRegisterUpdates(UpdateRegistrar* aRegistrar) override;
    void OnWorldDetached(world::RuntimeScene* aScene) override;
    void OnGamePrepared() override;

    void OnInitialize(const JobHandle& aJob) override;
    void OnUninitialize() override;

    // Exported to RTTI / CET.
    // Launches a mission by catalog id (e.g. "q113"). Captures live-session state into
    // ReplaySessionContext, then issues the gamedef load. Returns false if the id is
    // unknown or capture fails to find required systems.
    bool StartReplayById(CString aMissionId);

    // Stage caller-chosen facts onto the next replay's quest setup. Merged with the
    // selected mission's default preset at apply time.
    void SetQuestState(DynArray<Handle<ReplayFactDefinition>>& aFacts);

    // Cancel any pending replay context (does NOT reload a save — fire ReplayEnded for
    // an in-progress replay to abort cleanly).
    void ClearPendingReplay();

    // Mission catalog passthrough (lets CET drive a debug "list missions" without UI).
    std::uint32_t GetMissionCount();
    CString GetMissionId(std::uint32_t aIndex);
    CString GetMissionDisplayName(std::uint32_t aIndex);

    void AddRequest(EReplayRequestType aRequest) noexcept;

    static ReplayManager* GetInstance() noexcept;

    RTTI_IMPL_TYPEINFO(ReplayManager);
    RTTI_IMPL_ALLOCATOR();
};
} // namespace replay
