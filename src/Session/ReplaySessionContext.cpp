#include "ReplaySessionContext.hpp"

replay::ReplaySessionContext& replay::ReplaySessionContext::Get() noexcept
{
    static ReplaySessionContext s_instance{};
    return s_instance;
}

void replay::ReplaySessionContext::Reset() noexcept
{
    m_phase = EReplayPhase::Idle;
    m_mission = nullptr;
    m_characterCustomization = {};
    m_progression = {};
    m_inventory = {};
    m_returnTarget = {};
    m_stagedFacts.clear();
}
