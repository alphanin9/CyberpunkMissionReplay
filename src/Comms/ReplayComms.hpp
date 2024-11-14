#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/quest/NodeDefinition.hpp>

#include <Raw/Quest/NodeFuncs.hpp>
#include <Manager/ReplayManager.hpp>


using namespace Red;
namespace replay::Comms
{
void Setup() noexcept;
void Remove() noexcept;
char OnFactsDBManagerNodeDefinition(raw::Quest::FactsDBManager::ExecuteType aCallback, quest::NodeDefinition* aThis,
                                    void* aCtx, int64_t a3, DynArray<CName>& aOutSockets);

// Problem with communicating via game system is that game system can be NULL during execute call
// This is, understandably, a *problem*
inline SharedSpinLock s_replayRequestLock{};
inline DynArray<EReplayRequestType> s_replayRequests{};
}; // namespace ReplayComms