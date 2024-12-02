#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/ResourceLoader.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationState.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/GameDefinition.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/MainQuest.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/ISystemRequestsHandler.hpp>

#include <RED4ext/Scripting/Natives/gameGameSessionDesc.hpp>
#include <RED4ext/Scripting/Natives/worldWorldID.hpp>

#include <Shared/Raw/GameDefinition/GameDefinition.hpp>
#include <Shared/Raw/Ink/InkSystem.hpp>
#include <Shared/Raw/Quest/NodeFuncs.hpp>
#include <Shared/Util/Core.hpp>

#include "SessionLoader.hpp"

using namespace Red;

void session::GameLoader::LoadSavedGameByName(StringView aSaveName) noexcept
{
    shared::raw::Ink::SessionData::Data data{};

    auto handler = shared::raw::Ink::InkSystem::Get()->m_requestsHandler.Lock();
    auto inputDeviceId = shared::raw::Ink::SystemRequestsHandler::InputDeviceId::Ref(handler);

    game::GameSessionDesc sessionDesc{};

    sessionDesc.saveName = aSaveName.Data();

    data.AddArgument("gameSessionDesc", &sessionDesc);
    data.AddArgument("inputDeviceID", &inputDeviceId);

    shared::raw::Ink::SystemRequestsHandler::StartSession(handler, &data);
}

void session::GameLoader::LoadGameDefinitionByPath(GameDefinitionLoaderParams aParams) noexcept
{
    ResourceLoader::Get()
        ->LoadAsync(aParams.m_path)
        ->OnLoaded(
            [aParams](Handle<CResource>& aResource)
            {
                if (auto& definition = Cast<gsm::GameDefinition>(aResource))
                {
                    game::GameSessionDesc sessionDesc{};
                    shared::raw::GameDefinition::ToWorldID(definition, &sessionDesc.worldId);

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
                            MakeScriptedHandle<game::ui::CharacterCustomizationState>(
                                GetClass<game::ui::CharacterCustomizationState>());
                        sessionDesc.characterCustomizationState->GetType()->Assign(
                            sessionDesc.characterCustomizationState.instance,
                            aParams.m_characterCustomizationState.instance);
                    }

                    shared::raw::Ink::SessionData::Data data{};

                    data.AddArgument("gameSessionDesc", &sessionDesc);
                    data.AddArgument("spawnTags", &definition->spawnPointTags);

                    auto handler = shared::raw::Ink::InkSystem::Get()->m_requestsHandler.Lock();
                    auto inputDeviceId = shared::raw::Ink::SystemRequestsHandler::InputDeviceId::Ref(handler);

                    data.AddArgument("inputDeviceID", &inputDeviceId);

                    shared::raw::Ink::SystemRequestsHandler::StartSession(handler, &data);
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
        shared::raw::Ink::SystemRequestsHandler::ExitToMenu(shared::raw::Ink::InkSystem::Get()->m_requestsHandler.Lock());
    }

    static void TestUnknownSaveThing()
    {
        shared::raw::Ink::InkSystem::Get()->SetInitialLoadingScreenTDBID("InitLoadingScreen.LoadingScreenAfter113quest");
    }
};

RTTI_DEFINE_CLASS(SessionWrapper_TEST, {
    RTTI_METHOD(StartGameDefinitionFromPath);
    RTTI_METHOD(StartGameDefinitionFromPathWithState);
    RTTI_METHOD(LoadSaveByName);
    RTTI_METHOD(ExitToMainMenu);
    RTTI_METHOD(TestUnknownSaveThing);
});