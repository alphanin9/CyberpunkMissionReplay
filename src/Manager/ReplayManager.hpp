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
enum class EReplayGameDefinition : int
{
    Q113,
    Q115,
    Q306
};

enum class EReplayRequestType
{
    ReplayStarted,
    ReplayEnded
};

// Set up quest state via facts
// Theoretically use for launching different phases of quest as well (maybe straight to some fight?)
// For example for Q113:
// Did we get told about Jackie in Mikoshi?
// Is Takemura alive?
// Is Oda alive?
class ReplayFactDefinition : public IScriptable
{
public:
    CString m_factName{};
    int m_factValue{};

    void SetFactName(CString aName)
    {
        m_factName = aName;
    }

    void SetFactValue(int aValue)
    {
        m_factValue = aValue;
    }

    RTTI_IMPL_TYPEINFO(ReplayFactDefinition);
    RTTI_IMPL_ALLOCATOR();
};

class ReplayManager : public IGameSystem
{
    static constexpr auto c_useStaticProgressionBuild = true;
    static constexpr TweakDBID c_debugProgressionBuildTDBID = "ProgressionBuilds.VHard_50_RefBody";

    SharedSpinLock m_replayLock{};

    CString m_pointOfNoReturnId{};
    bool m_isLoadingReplay{};

    // Quest state we have defined by player settings
    DynArray<Handle<ReplayFactDefinition>> m_definedQuestState{};

    cp::PlayerSystem* m_playerSystem{};
    game::ScriptableSystemsContainer* m_scriptableSystemsContainer{};
    quest::QuestsSystem* m_questsSystem{};
    shared::raw::Ink::InkSystem* m_inkSystem{};

    ResourcePath GetGameDefinition(EReplayGameDefinition aDefinition) noexcept;

    // Setup correct quest state based on options
    void SetupQuestState() noexcept;

    // Setup correct player data based on options, maybe select progression build instead
    void SetupPlayerData() noexcept;

    // Setup correct inventory based on options, ignored in case of progression build
    void SetupInventory() noexcept;

    // Before starting game, we make PONR save (how?)
    void CapturePointOfNoReturnId() noexcept;

    void Tick(JobQueue& aQueue) noexcept;

public:
    void OnRegisterUpdates(UpdateRegistrar* aRegistrar) override;
    void OnWorldDetached(world::RuntimeScene* aScene) override;
    void OnGamePrepared() override;

    void OnInitialize(const JobHandle& aJob) override;
    void OnUninitialize() override;

    // Exported to RTTI
    // Starts game definition, sets things up for it so runtime doesn't need to think too much
    void StartReplayGameDefinition(EReplayGameDefinition aDefinition);

    // Exported to RTTI
    // Sets quest state based on player wishes
    void SetQuestState(DynArray<Handle<ReplayFactDefinition>>& aFacts);

    RTTI_IMPL_TYPEINFO(ReplayManager);
    RTTI_IMPL_ALLOCATOR();
};
} // namespace replay