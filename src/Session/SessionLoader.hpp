#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/StringView.hpp>

#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationState.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/GameDefinition.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/ISystemRequestsHandler.hpp>
#include <RED4ext/Scripting/Natives/worldWorldID.hpp>


using namespace Red;

namespace session
{
namespace GameLoader
{
// Params for game definition loader
// Passed by value to keep sanity
struct GameDefinitionLoaderParams
{
    // Gamedef path
    ResourcePath m_path{};
    // Initial loading screen TDBID, will eventually be used
    TweakDBID m_loadingScreen{};
    // Character customization state, can be null
    Handle<game::ui::CharacterCustomizationState> m_characterCustomizationState{};
};

void LoadSavedGameByName(StringView aSaveName) noexcept;
void LoadGameDefinitionByPath(GameDefinitionLoaderParams aParams) noexcept;
} // namespace GameLoader
} // namespace session