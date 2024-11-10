#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/StringView.hpp>

#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationState.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/GameDefinition.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/ISystemRequestsHandler.hpp>
#include <RED4ext/Scripting/Natives/worldWorldID.hpp>

#include <Raw/Ink/InkSystem.hpp>

using namespace Red;

namespace session
{
struct SessionData;

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

WeakHandle<ink::ISystemRequestsHandler> GetSystemRequestsHandler() noexcept;

void LoadSavedGameByName(StringView aSaveName) noexcept;
void LoadGameDefinitionByPath(GameDefinitionLoaderParams aParams) noexcept;
} // namespace GameLoader

enum class ESessionUnkEnum : std::uint8_t
{
    Unk1 = 3u
};

struct SessionData
{
    ESessionUnkEnum m_unk{}; // 00

    CString m_sessionName{};             // 08, set to "Session" in most of our usecases
    Handle<ISerializable> m_unkHandle{}; // 28, NULL during our usecase

    DynArray<SharedPtr<ISerializable>> m_arguments{}; // 38, has various args set inside of it
    // ISerializable type for this is actually wrong but w/e

    SessionData() noexcept;
    ~SessionData() noexcept;

    template<typename ObjectType>
    inline void AddArgument(CName aArg, ObjectType* aObject) noexcept
    {
        raw::Ink::SessionData::AddArgument(m_arguments, aArg, Red::GetType<ObjectType>(), aObject);
    }
};
} // namespace session