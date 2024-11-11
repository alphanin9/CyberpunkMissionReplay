#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

using namespace Red;

namespace replay
{
enum class EReplayGameDefinition : int
{
    ReplayTestGameDefinition = 0,
    BossRush = 1
};

enum class EReplayRequestType
{
    ReplayStarted,
    ReplayEnded
};

class ReplayManager : public IGameSystem
{
    SharedSpinLock m_replayLock{};

    SharedSpinLock m_requestLock{};
    DynArray<EReplayRequestType> m_requests{};

    CString m_pointOfNoReturnId{};
    bool m_isLoadingReplay{};

    ResourcePath GetGameDefinition(EReplayGameDefinition aDefinition) noexcept;

    void AddRequest(EReplayRequestType aRequest) noexcept;

    // Setup correct quest state based on options
    void SetupQuestState() noexcept;

    // Setup correct player data based on options, maybe select progression build instead
    void SetupPlayerData() noexcept;

    // Setup correct inventory based on options, ignored in case of progression build
    void SetupInventory() noexcept;

    void Tick(JobQueue& aQueue) noexcept;
public:
    void OnGamePrepared() override;
    void OnRegisterUpdates(UpdateRegistrar* aRegistrar) override;

    void OnInitialize(const JobHandle& aJob) override;
    void OnUninitialize() override;

    void CapturePointOfNoReturnId() noexcept;
    
    // Called by quest checkpoint node with specific debug string
    void OnReplayCommsStart() noexcept;

    // Exported to RTTI
    // Starts game definition, sets things up for it so runtime doesn't need to think too much
    void StartReplayGameDefinition(EReplayGameDefinition aDefinition);

    RTTI_IMPL_TYPEINFO(ReplayManager);
    RTTI_IMPL_ALLOCATOR();
};
}