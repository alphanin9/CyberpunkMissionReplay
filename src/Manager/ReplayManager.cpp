#include "ReplayManager.hpp"

#include <RED4ext/Scripting/Natives/Generated/game/TelemetryTelemetrySystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationSystem.hpp>
#include <Session/SessionLoader.hpp>

#include <Comms/ReplayComms.hpp>

#include <RED4ext/Scripting/Natives/Generated/quest/QuestsSystem.hpp>

#include <Shared/Raw/Ink/InkSystem.hpp>
#include <Shared/Raw/PlayerSystem/PlayerSystem.hpp>
#include <Shared/Raw/Quest/QuestsSystem.hpp>
#include <Shared/Raw/ScriptableSystem/ScriptableSystem.hpp>

#include <RED4ext/Scripting/Natives/Generated/quest/SetProgressionBuildRequest.hpp>

replay::ReplayManager* replay::ReplayManager::GetInstance() noexcept
{
    return s_this;
}

void replay::ReplayManager::AddRequest(EReplayRequestType aRequest) noexcept
{
    std::lock_guard _(m_replayRequestLock);

    m_replayRequests.PushBack(aRequest);
}

void replay::ReplayManager::OnGamePrepared()
{
    m_playerSystem = GetGameSystem<cp::PlayerSystem>();
    m_scriptableSystemsContainer = GetGameSystem<game::ScriptableSystemsContainer>();
    m_questsSystem = GetGameSystem<quest::QuestsSystem>();
    m_inkSystem = shared::raw::Ink::InkSystem::Get();

    // Placeholder, will be used for adding save PONR ID to persistent state/save
    if (m_isLoadingReplay)
    {
        // We've reached replay territory
        // Setup whatever we need

        m_isLoadingReplay = false;
    }
}

void replay::ReplayManager::OnWorldDetached(world::RuntimeScene* aScene)
{
    m_questsSystem = nullptr;
    m_playerSystem = nullptr;
    m_scriptableSystemsContainer = nullptr;
    // No point cleaning up ink system, it'll be the same throughout the game anyway
}

void replay::ReplayManager::Tick(JobQueue& aQueue) noexcept
{
    DynArray<EReplayRequestType> requests{};
    {
        std::lock_guard _(m_replayRequestLock);
        requests = std::move(m_replayRequests);
        m_replayRequests = DynArray<EReplayRequestType>{};
    }

    for (auto request : requests)
    {
        switch (request)
        {
        case EReplayRequestType::ReplayStarted:
            aQueue.Dispatch(
                [this]()
                {
                    if (!m_questsSystem)
                    {
                        return;
                    }

                    SetupQuestState();
                    SetupPlayerData();
                    SetupInventory();

                    // Tell quests system we're good and can continue
                    shared::raw::QuestsSystem::FactsDB(m_questsSystem)->SetFact("replay_init_finished", 1);
                });
            break;
        case EReplayRequestType::ReplayEnded:
            aQueue.Dispatch(
                [this]()
                {
                    if (m_inkSystem)
                    {
                        shared::raw::Ink::SystemRequestsHandler::ExitToMenu(m_inkSystem->m_requestsHandler.Lock());
                    }
                    // TODO: match with PONR ID
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
    s_this = this;

    Comms::Setup();
}

void replay::ReplayManager::OnUninitialize()
{
    Comms::Remove();
}

void replay::ReplayManager::CapturePointOfNoReturnId() noexcept
{
    struct TelemetryDataContainer
    {
        CString GetPointOfNoReturnID()
        {
            return shared::util::OffsetPtr<0xD8, CString>::Ref(this);
        }
    };

    auto telemetrySystem = GetGameSystem<TelemetrySystem>();
    auto dataContainer = shared::util::OffsetPtr<184, TelemetryDataContainer>::Ptr(telemetrySystem);

    if (!dataContainer)
    {
        m_pointOfNoReturnId = "";
        return;
    }

    m_pointOfNoReturnId = dataContainer->GetPointOfNoReturnID();
}

void replay::ReplayManager::SetupQuestState() noexcept
{
    DynArray<Handle<ReplayFactDefinition>> facts{};

    {
        std::unique_lock _(m_replayLock);
        facts = std::move(m_definedQuestState);
        m_definedQuestState = {};
    }

    auto& factsDB = shared::raw::QuestsSystem::FactsDB::Ref(m_questsSystem);

    for (auto& fact : facts)
    {
        factsDB.SetFact(fact->m_factName.c_str(), fact->m_factValue);
    }
}

void replay::ReplayManager::SetupPlayerData() noexcept
{
    // Player data is not currently setup for testing
    if (UseStaticProgressionBuild)
    {
        Handle<game::Object> playerObject{};

        shared::raw::PlayerSystem::GetPlayerControlledGameObject(m_playerSystem, playerObject);

        if (!playerObject)
        {
            return;
        }

        Handle<game::ScriptableSystem> playerDevelopmentSystem{};

        shared::raw::ScriptableSystemsContainer::GetSystemByName(m_scriptableSystemsContainer, playerDevelopmentSystem,
                                                                 "PlayerDevelopmentSystem");

        if (!playerDevelopmentSystem)
        {
            return;
        }

        auto setProgressionBuildReq = MakeHandle<quest::SetProgressionBuildRequest>();

        setProgressionBuildReq->owner = playerObject;
        setProgressionBuildReq->buildID = DebugProgressionBuildTDBID;

        shared::raw::ScriptableSystem::QueueRequest(playerDevelopmentSystem, setProgressionBuildReq);
    }
}

void replay::ReplayManager::SetupInventory() noexcept
{
}

ResourcePath replay::ReplayManager::GetGameDefinition(EReplayGameDefinition aDefinition) noexcept
{
    switch (aDefinition)
    {
    case EReplayGameDefinition::Q113:
        return R"(mod\quest\replay\q113\replay_q113.gamedef)";
    case EReplayGameDefinition::Q115:
        return R"(mod\quest\replay\q115\replay_q115.gamedef)";
    default:
        return "";
    }
}

void replay::ReplayManager::StartReplayGameDefinition(EReplayGameDefinition aDefinition)
{
    auto characterCustomizationSystem = GetGameSystem<game::ui::CharacterCustomizationSystem>();

    auto& characterCustomizationState =
        shared::util::OffsetPtr<0x78, Handle<game::ui::ICharacterCustomizationState>>::Ref(
            characterCustomizationSystem);

    session::GameLoader::GameDefinitionLoaderParams params{};

    params.m_path = GetGameDefinition(aDefinition);
    // params.m_loadingScreen = TweakDBID("InitialLoadingScreen.ReplayLoadingScreen");
    params.m_characterCustomizationState = Cast<game::ui::CharacterCustomizationState>(characterCustomizationState);

    m_isLoadingReplay = true;

    session::GameLoader::LoadGameDefinitionByPath(params);
}

void replay::ReplayManager::SetQuestState(DynArray<Handle<ReplayFactDefinition>>& aFacts)
{
    std::unique_lock _(m_replayLock);
    m_definedQuestState = aFacts; // IDK if we can std::move here without breaking Redscript/Redlib
}

RTTI_DEFINE_ENUM(replay::EReplayGameDefinition);
RTTI_DEFINE_CLASS(replay::ReplayManager, {
    RTTI_METHOD(StartReplayGameDefinition);
    RTTI_METHOD(SetQuestState);
});

RTTI_DEFINE_CLASS(replay::ReplayFactDefinition, {
    RTTI_METHOD(SetFactName);
    RTTI_METHOD(SetFactValue);
    RTTI_GETTER(m_factName);
    RTTI_GETTER(m_factValue);
})