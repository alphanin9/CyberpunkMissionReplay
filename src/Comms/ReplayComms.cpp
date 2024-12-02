#include "ReplayComms.hpp"

#include <RED4ext/Scripting/Natives/Generated/quest/FactsDBManagerNodeDefinition.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/SetVar_NodeType.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/QuestsSystem.hpp>

#include <Manager/ReplayManager.hpp>

#include <Shared/Hooks/HookManager.hpp>
#include <Shared/Raw/Quest/NodeFuncs.hpp>

void replay::Comms::Setup() noexcept
{
    shared::hook::HookWrap<shared::raw::Quest::FactsDBManager::Execute>(&OnFactsDBManagerNodeDefinition)
        .OrDie("Failed to hook FactsDBManager::Execute");
}

void replay::Comms::Remove() noexcept
{
    shared::hook::Unhook<shared::raw::Quest::FactsDBManager::Execute>();
}

char replay::Comms::OnFactsDBManagerNodeDefinition(shared::raw::Quest::FactsDBManager::ExecuteType aCallback,
                                                 quest::NodeDefinition* aThis, void* aCtx, int64_t a3,
                                                 DynArray<CName>& aOutSockets)
{
    // This func is also used by DynamicSpawnSystemNodeDefinition and RewardManagerNodeDefinition
    if (const auto factsDbManager = Cast<quest::FactsDBManagerNodeDefinition>(aThis))
    {
        if (const auto& setVar = Cast<quest::SetVar_NodeType>(factsDbManager->type))
        {
            if (setVar->value != 0 || !setVar->setExactValue)
            {
                return aCallback(aThis, aCtx, a3, aOutSockets);
            }

            constexpr auto c_replayCallPrefix = "REPLAY_CALL_HOOK_";

            std::string_view factName = setVar->factName.c_str();

            if (!factName.starts_with(c_replayCallPrefix))
            {
                return aCallback(aThis, aCtx, a3, aOutSockets);
            }

            factName.remove_prefix(std::char_traits<char>::length(c_replayCallPrefix));

            constexpr auto c_replayInitCommand = "INITIALIZE_ALL";
            constexpr auto c_replayExitCommand = "FINISHED";

            if (factName == c_replayInitCommand)
            {
                std::unique_lock _(s_replayRequestLock);
                s_replayRequests.PushBack(EReplayRequestType::ReplayStarted);

                aOutSockets.PushBack("ReplayInit");
                return 0;
            }
            else if (factName == c_replayExitCommand)
            {
                std::unique_lock _(s_replayRequestLock);
                s_replayRequests.PushBack(EReplayRequestType::ReplayEnded);

                aOutSockets.PushBack("ReplayExit");
                return 0;
            }
        }
    }

    return aCallback(aThis, aCtx, a3, aOutSockets);
}