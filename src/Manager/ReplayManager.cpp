#include "ReplayManager.hpp"

#include <RED4ext/Scripting/Natives/Generated/game/TelemetryTelemetrySystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/QuestsSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/SetProgressionBuildRequest.hpp>

#include <Capture/Inventory.hpp>
#include <Capture/PlayerProgression.hpp>
#include <Comms/ReplayComms.hpp>
#include <Manager/MissionCatalog.hpp>
#include <Session/ReplaySessionContext.hpp>
#include <Session/SessionLoader.hpp>
#include <Util/PluginLog.hpp>

#include <Shared/Raw/Ink/InkSystem.hpp>
#include <Shared/Raw/PlayerSystem/PlayerSystem.hpp>
#include <Shared/Raw/Quest/QuestsSystem.hpp>
#include <Shared/Raw/ScriptableSystem/ScriptableSystem.hpp>

namespace
{
using namespace Red;

// Telemetry data container layout, kept here until the PONR offsets are confirmed via
// RTTI/decompiled scripts (plan §0.2). Both reads are guarded so a layout drift just
// surfaces as a missing return target rather than a crash.
struct TelemetryDataContainer
{
    CString GetPointOfNoReturnID()
    {
        return shared::util::OffsetPtr<0xD8, CString>::Ref(this);
    }
};

CString ReadPointOfNoReturnId() noexcept
{
    auto telemetrySystem = GetGameSystem<TelemetrySystem>();
    if (!telemetrySystem)
        return {};
    auto dataContainer = shared::util::OffsetPtr<184, TelemetryDataContainer>::Ptr(telemetrySystem);
    if (!dataContainer)
        return {};
    return dataContainer->GetPointOfNoReturnID();
}

Handle<game::ui::CharacterCustomizationState> CaptureCharacterCustomization() noexcept
{
    auto system = GetGameSystem<game::ui::CharacterCustomizationSystem>();
    if (!system)
        return {};
    return shared::util::OffsetPtr<0x78, Handle<game::ui::ICharacterCustomizationState>>::Ref(system);
}
} // namespace

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
}

void replay::ReplayManager::OnWorldDetached(world::RuntimeScene* aScene)
{
    m_questsSystem = nullptr;
    m_playerSystem = nullptr;
    m_scriptableSystemsContainer = nullptr;
    // ink system persists for the process lifetime, no clear.
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
                        return;

                    auto& ctx = ReplaySessionContext::Get();
                    ctx.m_phase = EReplayPhase::InReplay;

                    SetupQuestState();
                    SetupPlayerData();
                    SetupInventory();

                    shared::raw::QuestsSystem::FactsDB(m_questsSystem)->SetFact("replay_init_finished", 1);
                });
            break;
        case EReplayRequestType::ReplayEnded:
            aQueue.Dispatch(
                [this]()
                {
                    auto& ctx = ReplaySessionContext::Get();
                    ctx.m_phase = EReplayPhase::Returning;
                    ResumeOrExit();
                    ctx.Reset();
                });
            break;
        }
    }
}

void replay::ReplayManager::OnRegisterUpdates(UpdateRegistrar* aRegistrar)
{
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

void replay::ReplayManager::CaptureReturnTarget() noexcept
{
    ReturnTarget target{};
    auto ponr = ReadPointOfNoReturnId();
    if (ponr.Length() > 0)
        target.m_pointOfNoReturnId = ponr.c_str();
    // Origin save name (most-recent save) is a verify-in-game item — leave empty for
    // now; ResumeOrExit() falls back to ExitToMenu when no target is set.
    // TODO[verify in-game]: enumerate save metadata via shared::raw::Save and pick the
    // most-recent slot, or stand up a dedicated "pre-replay" autosave (plan §6 #3).
    ReplaySessionContext::Get().m_returnTarget = std::move(target);
}

void replay::ReplayManager::ResumeOrExit() noexcept
{
    const auto& target = ReplaySessionContext::Get().m_returnTarget;
    if (!target.m_originSaveName.empty())
    {
        session::GameLoader::LoadSavedGameByName(target.m_originSaveName);
        return;
    }
    if (m_inkSystem)
        shared::raw::Ink::SystemRequestsHandler::ExitToMenu(m_inkSystem->m_requestsHandler.Lock());
}

void replay::ReplayManager::SetupQuestState() noexcept
{
    auto& ctx = ReplaySessionContext::Get();
    auto factsDB = shared::raw::QuestsSystem::FactsDB(m_questsSystem);
    if (!factsDB)
        return;

    // Mission-default preset first, then caller-staged overrides win on collision.
    if (ctx.m_mission)
    {
        for (std::uint32_t i = 0; i < ctx.m_mission->m_defaultFactCount; ++i)
        {
            const auto& preset = ctx.m_mission->m_defaultFacts[i];
            factsDB->SetFact(preset.m_factName, preset.m_factValue);
        }
    }
    for (const auto& staged : ctx.m_stagedFacts)
        factsDB->SetFact(staged.m_factName, staged.m_factValue);

    ctx.m_stagedFacts.clear();
}

void replay::ReplayManager::SetupPlayerData() noexcept
{
    if constexpr (kUseStaticProgressionBuild)
    {
        Handle<game::Object> playerObject{};
        shared::raw::PlayerSystem::GetPlayerControlledGameObject(m_playerSystem, playerObject);
        if (!playerObject)
            return;

        Handle<game::ScriptableSystem> playerDevelopmentSystem{};
        shared::raw::ScriptableSystemsContainer::GetSystemByName(m_scriptableSystemsContainer, playerDevelopmentSystem,
                                                                 "PlayerDevelopmentSystem");
        if (!playerDevelopmentSystem)
            return;

        auto setProgressionBuildReq = MakeHandle<quest::SetProgressionBuildRequest>();
        setProgressionBuildReq->owner = playerObject;
        setProgressionBuildReq->buildID = kDebugProgressionBuildTDBID;
        Handle<game::ScriptableSystemRequest> base = setProgressionBuildReq;
        shared::raw::ScriptableSystem::QueueRequest(playerDevelopmentSystem.instance, base);
        return;
    }

    PlayerProgression::Apply(ReplaySessionContext::Get().m_progression, m_playerSystem,
                             m_scriptableSystemsContainer);
}

void replay::ReplayManager::SetupInventory() noexcept
{
    if constexpr (kUseStaticProgressionBuild)
        return; // Static build path doesn't carry inventory.
    Inventory::Apply(ReplaySessionContext::Get().m_inventory, m_playerSystem, m_scriptableSystemsContainer);
}

bool replay::ReplayManager::StartReplayById(CString aMissionId)
{
    auto entry = MissionCatalog::FindById(aMissionId.c_str());
    if (!entry)
    {
        log::Warn("StartReplayById: unknown mission id");
        return false;
    }

    auto& ctx = ReplaySessionContext::Get();
    ctx.Reset();
    ctx.m_mission = entry;
    ctx.m_phase = EReplayPhase::CapturingSource;

    ctx.m_characterCustomization = CaptureCharacterCustomization();
    if constexpr (!kUseStaticProgressionBuild)
    {
        ctx.m_progression = PlayerProgression::Capture();
        ctx.m_inventory = Inventory::Capture();
    }
    CaptureReturnTarget();

    session::GameLoader::GameDefinitionLoaderParams params{};
    params.m_path = ResourcePath(entry->m_gameDefPath);
    params.m_characterCustomizationState =
        Cast<game::ui::CharacterCustomizationState>(ctx.m_characterCustomization);

    ctx.m_phase = EReplayPhase::LoadingReplay;
    session::GameLoader::LoadGameDefinitionByPath(params);
    return true;
}

void replay::ReplayManager::SetQuestState(DynArray<Handle<ReplayFactDefinition>>& aFacts)
{
    std::vector<ReplayFactPreset> staged{};
    staged.reserve(aFacts.size());
    for (uint32_t i = 0; i < aFacts.size(); ++i)
    {
        if (!aFacts[i])
            continue;
        // Store the fact name strings on the ReplayFactDefinition handles themselves,
        // which the context keeps alive via the staged-facts vector. The preset entries
        // are views into stable CString storage on the ReplayFactDefinition objects.
        staged.push_back({aFacts[i]->m_factName.c_str(), aFacts[i]->m_factValue});
    }
    ReplaySessionContext::Get().m_stagedFacts = std::move(staged);
}

void replay::ReplayManager::ClearPendingReplay()
{
    ReplaySessionContext::Get().Reset();
}

std::uint32_t replay::ReplayManager::GetMissionCount()
{
    return MissionCatalog::Count();
}

CString replay::ReplayManager::GetMissionId(std::uint32_t aIndex)
{
    auto entry = MissionCatalog::FindByIndex(aIndex);
    return entry ? CString(entry->m_id) : CString{};
}

CString replay::ReplayManager::GetMissionDisplayName(std::uint32_t aIndex)
{
    auto entry = MissionCatalog::FindByIndex(aIndex);
    return entry ? CString(entry->m_displayName) : CString{};
}

RTTI_DEFINE_CLASS(replay::ReplayManager, {
    RTTI_METHOD(StartReplayById);
    RTTI_METHOD(SetQuestState);
    RTTI_METHOD(ClearPendingReplay);
    RTTI_METHOD(GetMissionCount);
    RTTI_METHOD(GetMissionId);
    RTTI_METHOD(GetMissionDisplayName);
});

RTTI_DEFINE_CLASS(replay::ReplayFactDefinition, {
    RTTI_METHOD(SetFactName);
    RTTI_METHOD(SetFactValue);
    RTTI_GETTER(m_factName);
    RTTI_GETTER(m_factValue);
});
