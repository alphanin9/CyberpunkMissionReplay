#pragma once
#include <Detail/AddressHashes.hpp>
#include <Util/Core.hpp>

#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/ink/ISystemRequestsHandler.hpp>

#include <RED4ext/Scripting/Natives/Generated/ink/FullScreenLayer.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/LoadingLayer.hpp>
#include <RED4ext/Scripting/Natives/inkLayer.hpp>
#include <RED4ext/Scripting/Natives/inkWidget.hpp>

#include <RED4ext/Scripting/Natives/Generated/ink/InitialLoadingScreenSaveData.hpp>

using namespace Red;

namespace raw::Ink
{

// https:// github.com/psiberx/cp2077-codeware/blob/main/src/Red/InkSystem.hpp
struct InkLayerManager
{
    uint8_t unk00[0x38];                 // 00
    DynArray<Handle<ink::Layer>> layers; // 38

    Handle<ink::LoadingLayer> GetLoadingLayer() noexcept;
};
RED4EXT_ASSERT_OFFSET(InkLayerManager, layers, 0x38);

struct InkSystem
{
    static InkSystem* Get() noexcept;
    InkLayerManager* GetLayerManager() noexcept;

    void SetInitialLoadingScreenTDBID(TweakDBID aId) noexcept;

    uint8_t unk00[0x2E8];                                      // 000
    WeakHandle<ink::Widget> m_inputWidget;                     // 2E8
    uint16_t m_keyboardState;                                  // 2F8
    uint8_t unk2FA[0x368 - 0x2FA];                             // 2FA
    WeakHandle<ink::ISystemRequestsHandler> m_requestsHandler; // 368
    DynArray<SharedPtr<InkLayerManager>> m_layerManagers;      // 378
};
RED4EXT_ASSERT_OFFSET(InkSystem, m_requestsHandler, 0x368);
RED4EXT_ASSERT_OFFSET(InkSystem, m_layerManagers, 0x378);

constexpr auto InkSystemInstance = Util::RawPtr<UsedAddressHashes::InkSystem_Instance, InkSystem*>();

namespace SystemRequestsHandler
{
using InputDeviceId = Util::OffsetPtr<0x570, std::uint64_t>;
constexpr auto StartSession = Util::RawFunc<UsedAddressHashes::InkSystemRequestsHandler_StartSession,
                                            void* (*)(ink::ISystemRequestsHandler*, void*)>();
constexpr auto ExitToMenu = Util::RawFunc<UsedAddressHashes::InkSystemRequestsHandler_ExitToMainMenu,
                                          void* (*)(ink::ISystemRequestsHandler*)>();
}; // namespace SystemRequestsHandler

namespace LoadingScreen
{
constexpr auto SetUnknownVarInInitialLoadingScreen =
    Util::RawFunc<UsedAddressHashes::InkLoadingLayer_SetUnknownVar, void (*)(void*, uint32_t)>();

using LoadingScreenTDBID = Util::OffsetPtr<0x208, TweakDBID>;
};

namespace SessionData
{
constexpr auto AddArgument =
    Util::RawFunc<UsedAddressHashes::SessionData_AddArgumentToList,
                  void* (*)(DynArray<SharedPtr<ISerializable>>&, CName, CBaseRTTIType*, void*)>();
constexpr auto Dtor = Util::RawFunc<UsedAddressHashes::SessionData_dtor, void* (*)(void*)>();
}; // namespace SessionData
}; // namespace raw::Ink