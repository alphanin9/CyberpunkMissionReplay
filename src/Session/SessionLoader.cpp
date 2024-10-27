#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/GameSessionDesc.hpp>
#include <RED4ext/Scripting/Natives/Generated/gsm/GameDefinition.hpp>

#include <Detail/AddressHashes.hpp>

using namespace Red;

namespace Session {
	enum class ESessionUnkEnum : std::uint8_t {
		Unk1 = 3u
	};

	struct SessionData {
		ESessionUnkEnum m_unk{}; // 00
		
		CString m_sessionName{}; // 08, set to "Session" in most things
		Handle<ISerializable> m_unkHandle{}; // 28, NULL during our usecase
		
		DynArray<SharedPtr<ISerializable>> m_arguments{}; // 38, has various args set inside of it

		// We use the game's own dtor for this, no point caring about what the game does

		~SessionData() {
            static const auto func =
                UniversalRelocFunc<int64_t (*)(SessionData* aThis)>(UsedAddressHashes::SessionData_dtor);

			func(this);
		}
	};

	void SetupSessionDataFromGameDefinition(SessionData& aData, Handle<gsm::GameDefinition> aGameDef)
    {
		// To do, this is actually hard
	}

	void SetupSessionDataFromSaveName(SessionData& aData, const CString& aSaveName)
    {
		
	}
    }