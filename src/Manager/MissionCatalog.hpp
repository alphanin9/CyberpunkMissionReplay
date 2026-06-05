#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

using namespace Red;

namespace replay
{
struct ReplayFactPreset
{
    const char* m_factName;
    int m_factValue;
};

// Per-mission entry. Default facts are baked in here so missions stay self-describing.
// TweakDB-record or JSON sourcing can replace the static table later (see plan §1.1).
struct MissionCatalogEntry
{
    const char* m_id;
    const char* m_displayName;
    const char* m_gameDefPath;
    const ReplayFactPreset* m_defaultFacts;
    std::uint32_t m_defaultFactCount;
};

namespace MissionCatalog
{
const MissionCatalogEntry* FindById(std::string_view aId) noexcept;
const MissionCatalogEntry* FindByIndex(std::uint32_t aIndex) noexcept;
std::uint32_t Count() noexcept;
} // namespace MissionCatalog
} // namespace replay
