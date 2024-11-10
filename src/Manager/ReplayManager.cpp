#include "ReplayManager.hpp"

#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/TelemetryTelemetrySystem.hpp>
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

void replay::ReplayManager::OnInitialize(const JobHandle& aJobHandle)
{
    ReplayComms::Setup();
}

void replay::ReplayManager::OnUninitialize()
{
    ReplayComms::Remove();
}

void replay::ReplayManager::CapturePointOfNoReturnId() noexcept
{
    struct TelemetryDataContainer
    {
        CString GetPointOfNoReturnID()
        {
            return Util::OffsetPtr<0xD8, CString>::Ref(this);
        }
    };

    auto telemetrySystem = GetGameSystem<TelemetrySystem>();
    auto dataContainer = Util::OffsetPtr<184, TelemetryDataContainer>::Ptr(telemetrySystem);

    if (!dataContainer)
    {
        m_pointOfNoReturnId = "";
        return;
    }

    m_pointOfNoReturnId = dataContainer->GetPointOfNoReturnID();
}

void replay::ReplayManager::OnReplayCommsStart() noexcept
{
    // Checkpoint node is called, do something
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
        Util::OffsetPtr<0x78, Handle<game::ui::ICharacterCustomizationState>>::Ref(characterCustomizationSystem);

    session::GameLoader::GameDefinitionLoaderParams params{};

    params.m_path = GetGameDefinition(aDefinition);
    // params.m_loadingScreen = TweakDBID("InitialLoadingScreen.ReplayLoadingScreen");
    params.m_characterCustomizationState = Cast<game::ui::CharacterCustomizationState>(characterCustomizationState);

    m_isLoadingReplay = true;

    session::GameLoader::LoadGameDefinitionByPath(params);
}

RTTI_DEFINE_ENUM(replay::EReplayGameDefinition);
RTTI_DEFINE_CLASS(replay::ReplayManager, { RTTI_METHOD(StartReplayGameDefinition); });