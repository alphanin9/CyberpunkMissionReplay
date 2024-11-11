#include "ReplayManager.hpp"

#include <RED4ext/Scripting/Natives/Generated/game/TelemetryTelemetrySystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationSystem.hpp>
#include <Session/SessionLoader.hpp>
#include <Util/Core.hpp>

#include <Comms/ReplayComms.hpp>

void replay::ReplayManager::OnGamePrepared()
{
    // Placeholder, will be used for adding save PONR ID to persistent state
    if (m_isLoadingReplay)
    {
        // We've reached replay territory
        // Setup whatever we need

        m_isLoadingReplay = false;
    }
}

void replay::ReplayManager::Tick(JobQueue& aQueue) noexcept
{
    DynArray<EReplayRequestType> requests{};
    {
        std::unique_lock _(m_requestLock);
        requests = std::move(m_requests);
        m_requests = DynArray<EReplayRequestType>();
    }

    for (auto request : requests)
    {
        switch (request)
        {
        case EReplayRequestType::ReplayStarted:
            aQueue.Dispatch(
                [this]()
                {
                    SetupQuestState();
                    SetupPlayerData();
                    SetupInventory();

                    // Tell quests system we're good and can continue
                });
            break;
        case EReplayRequestType::ReplayEnded:
            aQueue.Dispatch(
                [this]() {

                });
            break;
        }
    }
}

void replay::ReplayManager::OnRegisterUpdates(UpdateRegistrar* aRegistrar)
{
    // Update after quests system, makes more sense
    aRegistrar->RegisterUpdate(UpdateTickGroup::PostBuckets, this, "ReplayManager/Tick",
                               [this](FrameInfo& aInfo, JobQueue& aQueue) { Tick(aQueue); });
}

void replay::ReplayManager::OnInitialize(const JobHandle& aJobHandle)
{
    replay::Comms::Setup();
}

void replay::ReplayManager::OnUninitialize()
{
    replay::Comms::Remove();
}

void replay::ReplayManager::AddRequest(EReplayRequestType aRequest)
{
    std::unique_lock _(m_requestLock);
    m_requests.PushBack(aRequest);
}

void replay::ReplayManager::CapturePointOfNoReturnId() noexcept
{
    struct TelemetryDataContainer
    {
        CString GetPointOfNoReturnID()
        {
            return util::OffsetPtr<0xD8, CString>::Ref(this);
        }
    };

    auto telemetrySystem = GetGameSystem<TelemetrySystem>();
    auto dataContainer = util::OffsetPtr<184, TelemetryDataContainer>::Ptr(telemetrySystem);

    if (!dataContainer)
    {
        m_pointOfNoReturnId = "";
        return;
    }

    m_pointOfNoReturnId = dataContainer->GetPointOfNoReturnID();
}

void replay::ReplayManager::OnReplayCommsStart() noexcept
{
    // Checkpoint node sent us a request, do sth
}

void replay::ReplayManager::SetupQuestState() noexcept
{
}

void replay::ReplayManager::SetupPlayerData() noexcept
{
}

void replay::ReplayManager::SetupInventory() noexcept
{
}

ResourcePath replay::ReplayManager::GetGameDefinition(EReplayGameDefinition aDefinition) noexcept
{
    switch (aDefinition)
    {
    case EReplayGameDefinition::ReplayTestGameDefinition:
        return R"(mod\replay\test\replay_000_test.gamedef)";
    case EReplayGameDefinition::BossRush:
        return R"(mod\quest\replay\boss_rush\replay_boss_rush.gamedef)";
    default:
        return "";
    }
}

void replay::ReplayManager::StartReplayGameDefinition(EReplayGameDefinition aDefinition)
{
    auto characterCustomizationSystem = GetGameSystem<game::ui::CharacterCustomizationSystem>();

    auto& characterCustomizationState =
        util::OffsetPtr<0x78, Handle<game::ui::ICharacterCustomizationState>>::Ref(characterCustomizationSystem);

    session::GameLoader::GameDefinitionLoaderParams params{};

    params.m_path = GetGameDefinition(aDefinition);
    // params.m_loadingScreen = TweakDBID("InitialLoadingScreen.ReplayLoadingScreen");
    params.m_characterCustomizationState = Cast<game::ui::CharacterCustomizationState>(characterCustomizationState);

    m_isLoadingReplay = true;

    session::GameLoader::LoadGameDefinitionByPath(params);
}

RTTI_DEFINE_ENUM(replay::EReplayGameDefinition);
RTTI_DEFINE_CLASS(replay::ReplayManager, { RTTI_METHOD(StartReplayGameDefinition); });