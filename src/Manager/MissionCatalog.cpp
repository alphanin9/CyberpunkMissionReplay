#include "MissionCatalog.hpp"

#include <array>

namespace
{
using namespace replay;

// Per-mission default fact presets. Populated as the quest authoring catches up; an
// empty preset is fine for missions whose `*.quest` graph does all setup itself.
constexpr std::array<ReplayFactPreset, 0> kNoFacts{};

constexpr std::array<MissionCatalogEntry, 4> kCatalog{{
    {
        "q113",
        "Devil ending",
        R"(mod\quest\replay\q113\replay_q113.gamedef)",
        kNoFacts.data(),
        static_cast<std::uint32_t>(kNoFacts.size()),
    },
    {
        "q115",
        "Rogue ending",
        R"(mod\quest\replay\q115\replay_q115.gamedef)",
        kNoFacts.data(),
        static_cast<std::uint32_t>(kNoFacts.size()),
    },
    {
        "q306",
        "Phantom Liberty - Songbird path",
        R"(mod\quest\replay\q306\replay_q306.gamedef)",
        kNoFacts.data(),
        static_cast<std::uint32_t>(kNoFacts.size()),
    },
    {
        "boss_rush",
        "Boss rush (Royce + mall)",
        R"(mod\quest\replay\boss_rush\replay_boss_rush.gamedef)",
        kNoFacts.data(),
        static_cast<std::uint32_t>(kNoFacts.size()),
    },
}};
} // namespace

const replay::MissionCatalogEntry* replay::MissionCatalog::FindById(std::string_view aId) noexcept
{
    for (const auto& entry : kCatalog)
    {
        if (aId == entry.m_id)
            return &entry;
    }
    return nullptr;
}

const replay::MissionCatalogEntry* replay::MissionCatalog::FindByIndex(std::uint32_t aIndex) noexcept
{
    if (aIndex >= kCatalog.size())
        return nullptr;
    return &kCatalog[aIndex];
}

std::uint32_t replay::MissionCatalog::Count() noexcept
{
    return static_cast<std::uint32_t>(kCatalog.size());
}
