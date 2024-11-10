#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/ResourceLoader.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationState.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/GameDefinition.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/MainQuest.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/ISystemRequestsHandler.hpp>

#include <RED4ext/Scripting/Natives/gameGameSessionDesc.hpp>
#include <RED4ext/Scripting/Natives/worldWorldID.hpp>

#include <Detail/AddressHashes.hpp>
#include <Raw/GameDefinition/GameDefinition.hpp>
#include <Raw/Ink/InkSystem.hpp>
#include <Raw/Quest/NodeFuncs.hpp>
#include <Util/Core.hpp>

#include "SessionLoader.hpp"

using namespace Red;

session::SessionData::~SessionData() noexcept
{
    raw::Ink::SessionData::Dtor(this);
}

session::SessionData::SessionData() noexcept
    : m_unk(ESessionUnkEnum::Unk1)
    , m_sessionName("Session")
    , m_arguments()
{
}

WeakHandle<ink::ISystemRequestsHandler> session::GameLoader::GetSystemRequestsHandler() noexcept
{
    return raw::Ink::InkSystem::Get()->m_requestsHandler;
}

void session::GameLoader::LoadSavedGameByName(StringView aSaveName) noexcept
{
    SessionData data{};

    auto handler = GetSystemRequestsHandler().Lock();
    auto inputDeviceId = raw::Ink::SystemRequestsHandler::InputDeviceId::Ref(handler);

    game::GameSessionDesc sessionDesc{};

    sessionDesc.unk38 = true;
    sessionDesc.unk40 = 7u;
    sessionDesc.unk44 = 257u;
    sessionDesc.unk68 = std::numeric_limits<std::uint32_t>::max();
    sessionDesc.playerSpawnParams.spawnPoint.orientation.r = 1.0f;

    sessionDesc.saveName = aSaveName.Data();

    data.AddArgument("gameSessionDesc", &sessionDesc);
    data.AddArgument("inputDeviceID", &inputDeviceId);

    raw::Ink::SystemRequestsHandler::StartSession(handler, &data);
}

void session::GameLoader::LoadGameDefinitionByPath(GameDefinitionLoaderParams aParams) noexcept
{
    // TODO: set up loading screen like game does it
    // Looks like shared ptr/handle ctor somewhere in StartNewGame with TDBID set ptr + 8

    ResourceLoader::Get()
        ->LoadAsync(aParams.m_path)
        ->OnLoaded(
            [aParams](Handle<CResource>& aResource)
            {
                if (auto& definition = Cast<gsm::GameDefinition>(aResource))
                {
                    game::GameSessionDesc sessionDesc{};

                    // These need to be set up for things to go right

                    sessionDesc.unk38 = true;
                    sessionDesc.unk3C = 0u;
                    sessionDesc.unk40 = 7u;
                    sessionDesc.unk44 = 257u;
                    sessionDesc.unk68 = std::numeric_limits<std::uint32_t>::max();

                    sessionDesc.playerSpawnParams.spawnPoint.orientation.r = 1.0f;

                    raw::GameDefinition::ToWorldID(definition, &sessionDesc.worldId);

                    for (auto& quest : definition->mainQuests)
                    {
                        game::MainQuestData questData{};

                        questData.questPath = quest->questFile.path;
                        questData.questPathValid = static_cast<bool>(quest->questFile.path);

                        if (quest->additionalContent)
                        {
                            questData.additionalContentId = quest->additionalContentName;
                        }

                        sessionDesc.mainQuests.PushBack(questData);
                    }

                    if (aParams.m_characterCustomizationState)
                    {
                        sessionDesc.characterCustomizationState =
                            MakeScriptedHandle<game::ui::CharacterCustomizationState>(GetClass<game::ui::CharacterCustomizationState>());
                        sessionDesc.characterCustomizationState->GetType()->Assign(
                            sessionDesc.characterCustomizationState.instance,
                            aParams.m_characterCustomizationState.instance);
                    }

                    SessionData data{};

                    data.AddArgument("gameSessionDesc", &sessionDesc);
                    data.AddArgument("spawnTags", &definition->spawnPointTags);

                    auto handler = GetSystemRequestsHandler().Lock();
                    auto inputDeviceId = raw::Ink::SystemRequestsHandler::InputDeviceId::Ref(handler);

                    data.AddArgument("inputDeviceID", &inputDeviceId);

                    raw::Ink::SystemRequestsHandler::StartSession(handler, &data);
                }
            });
}

class SessionWrapper_TEST
{
public:
    static void StartGameDefinitionFromPath(CString& aPath)
    {
        session::GameLoader::GameDefinitionLoaderParams params{};

        params.m_path = aPath.c_str();

        session::GameLoader::LoadGameDefinitionByPath(params);
    }

    static void StartGameDefinitionFromPathWithState(CString& aPath,
                                                     Handle<game::ui::CharacterCustomizationState>& aState)
    {
        session::GameLoader::GameDefinitionLoaderParams params{};

        params.m_path = aPath.c_str();
        params.m_characterCustomizationState = aState;

        session::GameLoader::LoadGameDefinitionByPath(params);
    }

    static void LoadSaveByName(CString& aPath)
    {
        session::GameLoader::LoadSavedGameByName(aPath);
    }

    static void ExitToMainMenu()
    {
        raw::Ink::SystemRequestsHandler::ExitToMenu(session::GameLoader::GetSystemRequestsHandler().Lock());
    }

    static void TestUnknownSaveThing()
    {
        raw::Ink::InkSystem::Get()->SetInitialLoadingScreenTDBID("InitLoadingScreen.LoadingScreenAfter113quest");
    }
};

RTTI_DEFINE_CLASS(SessionWrapper_TEST, {
    RTTI_METHOD(StartGameDefinitionFromPath);
    RTTI_METHOD(StartGameDefinitionFromPathWithState);
    RTTI_METHOD(LoadSaveByName);
    RTTI_METHOD(ExitToMainMenu);
    RTTI_METHOD(TestUnknownSaveThing);
});