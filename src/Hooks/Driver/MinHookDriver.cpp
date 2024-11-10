#include <MinHook.h>

#include "MinHookDriver.hpp"

Hooks::MinHookDriver::MinHookDriver()
{
    MH_Initialize();
}

Hooks::MinHookDriver::~MinHookDriver()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

Hooks::MinHookDriver& Hooks::MinHookDriver::Get() noexcept
{
    static MinHookDriver instance{};

    return instance;
}

bool Hooks::MinHookDriver::Attach(uintptr_t aAddr, void* aDetour) const noexcept
{
    return Attach(aAddr, aDetour, nullptr);
}

bool Hooks::MinHookDriver::Attach(uintptr_t aAddr, void* aDetour, void** aOriginalFn) const noexcept
{
    if (MH_CreateHook(reinterpret_cast<void*>(aAddr), aDetour, aOriginalFn) != MH_OK)
        return false;

    if (MH_EnableHook(reinterpret_cast<void*>(aAddr)) != MH_OK)
    {
        MH_RemoveHook(reinterpret_cast<void*>(aAddr));
        return false;
    }

    return true;
}

bool Hooks::MinHookDriver::Detach(uintptr_t aAddr) const noexcept
{
    return MH_DisableHook(reinterpret_cast<void*>(aAddr)) == MH_OK &&
           MH_RemoveHook(reinterpret_cast<void*>(aAddr)) == MH_OK;
}