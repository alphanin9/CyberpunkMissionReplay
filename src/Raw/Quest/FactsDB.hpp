#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <cstdint>
#include <Util/Core.hpp>

namespace raw
{
namespace Facts
{
struct FactKey
{
    std::uint32_t m_hash{};

    constexpr FactKey(const char* aStr) noexcept
        : m_hash(FNV1a32(aStr))
    {
    }

    operator std::uint32_t() const noexcept
    {
        return m_hash;
    }
};

struct FactStore
{
    std::uint64_t unk00;
    Red::Map<FactKey, int> m_data;
};

struct FactsDBManager
{
    static constexpr auto c_namedFacts = 1u;
    static constexpr auto c_maxFactsTables = 11u;

    virtual ~FactsDBManager() = 0;                                             // 00
    virtual void sub_08() = 0;                                                 // 08
    virtual int32_t GetFact(std::uint32_t aStore, FactKey aFact) = 0;          // 10
    virtual void SetFact(std::uint32_t aStore, FactKey aFact, int aValue) = 0; // 18

    inline int GetFact(FactKey aFact)
    {
        return GetFact(c_namedFacts, aFact);
    }

    inline void SetFact(FactKey aFact, int aValue)
    {
        SetFact(c_namedFacts, aFact, aValue);
    }

    FactStore* m_factStorage[c_maxFactsTables];
};
} // namespace Facts

namespace Quest
{
using FactsDB = Util::OffsetPtr<0xF8, Facts::FactsDBManager*>;
}
} // namespace raw