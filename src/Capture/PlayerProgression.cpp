#include "PlayerProgression.hpp"

#include <RED4ext/RTTITypes.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ScriptableSystemRequest.hpp>
#include <RED4ext/Scripting/Natives/Generated/quest/AddDevelopmentPointsRequest.hpp>

#include <Shared/Raw/PlayerSystem/PlayerSystem.hpp>
#include <Shared/Raw/ScriptableSystem/ScriptableSystem.hpp>

#include <Util/PluginLog.hpp>

namespace
{
using namespace Red;
using namespace replay;

// Mirror of `public struct SAttribute` (cyberpunk/orphans.swift) — no SDK header.
struct SAttribute
{
    game::data::StatType attributeName{};
    std::int32_t value{};
    TweakDBID id{};
};

constexpr CName kPlayerDevelopmentSystem = "PlayerDevelopmentSystem";
// CClass::GetFunction takes a short name and resolves to the first registered overload —
// stick to single-signature getters where possible.
constexpr CName kGetAttributes = "GetAttributes";
constexpr CName kIsNewPerkBought = "IsNewPerkBought"; // (perkType: gamedataNewPerkType) -> Int32
constexpr CName kGetProficiencyLevel = "GetProficiencyLevel";
constexpr CName kGetTraitLevel = "GetTraitLevel";
constexpr CName kGetDevPoints = "GetDevPoints";

Handle<game::ScriptableSystem> GetPlayerDevelopmentSystem(game::ScriptableSystemsContainer* aContainer) noexcept
{
    Handle<game::ScriptableSystem> system{};
    if (!aContainer)
        return system;
    shared::raw::ScriptableSystemsContainer::GetSystemByName(aContainer, system, kPlayerDevelopmentSystem);
    return system;
}

Handle<game::Object> GetPlayerObject(cp::PlayerSystem* aPlayerSystem) noexcept
{
    Handle<game::Object> obj{};
    if (aPlayerSystem)
        shared::raw::PlayerSystem::GetPlayerControlledGameObject(aPlayerSystem, obj);
    return obj;
}

// Iterate every value of an RTTI enum, calling visitor(int64Value). Used to enumerate
// gamedataNewPerkType / gamedataProficiencyType / gamedataTraitType /
// gamedataDevelopmentPointType for getter-based capture. Skips obvious sentinel names
// (Invalid / Count) to avoid spurious zero entries.
template<typename F>
void ForEachEnumValue(CName aEnumName, F&& aVisitor)
{
    auto type = CRTTISystem::Get()->GetType(aEnumName);
    if (!type || type->GetType() != ERTTIType::Enum)
        return;

    auto enumType = static_cast<CEnum*>(type);
    const auto count = enumType->valueList.size();
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto name = enumType->hashList[i].ToString();
        if (!name)
            continue;
        const std::string_view nameView{name};
        if (nameView == "Invalid" || nameView == "Count" || nameView == "None")
            continue;
        aVisitor(enumType->valueList[i]);
    }
}

// PDS getters are script methods, so go through RTTI. Returns 0 on failure.
std::int32_t QueryInt(const Handle<game::ScriptableSystem>& aSystem, CName aFuncName) noexcept
{
    std::int32_t value = 0;
    CallVirtual(aSystem.instance, aFuncName, value);
    return value;
}

template<typename Arg>
std::int32_t QueryIntWithArg(const Handle<game::ScriptableSystem>& aSystem, CName aFuncName, Arg aArg) noexcept
{
    std::int32_t value = 0;
    CallVirtual(aSystem.instance, aFuncName, value, aArg);
    return value;
}

void CaptureAttributes(const Handle<game::ScriptableSystem>& aSystem, PlayerProgressionSnapshot& aOut) noexcept
{
    DynArray<SAttribute> attributes{};
    if (!CallVirtual(aSystem.instance, kGetAttributes, attributes))
        return;
    aOut.m_attributes.reserve(attributes.size());
    for (uint32_t i = 0; i < attributes.size(); ++i)
    {
        aOut.m_attributes.push_back({attributes[i].attributeName, attributes[i].value});
    }
}

void CaptureNewPerks(const Handle<game::ScriptableSystem>& aSystem, PlayerProgressionSnapshot& aOut) noexcept
{
    ForEachEnumValue("gamedataNewPerkType",
                     [&](std::int64_t aValue)
                     {
                         const auto perkType = static_cast<game::data::NewPerkType>(aValue);
                         const auto level = QueryIntWithArg(aSystem, kIsNewPerkBought, perkType);
                         if (level > 0)
                             aOut.m_newPerks.push_back({perkType, level});
                     });
}

void CaptureProficiencies(const Handle<game::ScriptableSystem>& aSystem, PlayerProgressionSnapshot& aOut) noexcept
{
    ForEachEnumValue("gamedataProficiencyType",
                     [&](std::int64_t aValue)
                     {
                         const auto type = static_cast<game::data::ProficiencyType>(aValue);
                         const auto level = QueryIntWithArg(aSystem, kGetProficiencyLevel, type);
                         if (level > 0)
                             aOut.m_proficiencies.push_back({type, level});
                     });
}

void CaptureTraits(const Handle<game::ScriptableSystem>& aSystem, PlayerProgressionSnapshot& aOut) noexcept
{
    ForEachEnumValue("gamedataTraitType",
                     [&](std::int64_t aValue)
                     {
                         const auto type = static_cast<game::data::TraitType>(aValue);
                         const auto level = QueryIntWithArg(aSystem, kGetTraitLevel, type);
                         if (level > 0)
                             aOut.m_traits.push_back({type, level});
                     });
}

void CaptureDevPoints(const Handle<game::ScriptableSystem>& aSystem, PlayerProgressionSnapshot& aOut) noexcept
{
    ForEachEnumValue("gamedataDevelopmentPointType",
                     [&](std::int64_t aValue)
                     {
                         const auto type = static_cast<game::data::DevelopmentPointType>(aValue);
                         const auto unspent = QueryIntWithArg(aSystem, kGetDevPoints, type);
                         if (unspent > 0)
                             aOut.m_devPoints.push_back({type, unspent});
                     });
}

// Build a Player-Scriptable-System request by RTTI name (script-only types like
// SetAttribute / BuyNewPerk / IncreaseTraitLevel / SetProficiencyLevel have no SDK
// header). Caller fills in the request's fields via Red::GetProperty before queueing.
Handle<game::ScriptableSystemRequest> MakeRequest(CName aTypeName) noexcept
{
    auto handle = MakeScriptedHandle<game::ScriptableSystemRequest>(aTypeName);
    if (!handle)
        replay::log::Warn("PlayerProgression::Apply: failed to construct request");
    return handle;
}

template<typename T>
void SetReqProp(const Handle<game::ScriptableSystemRequest>& aRequest, CName aProp, T&& aValue) noexcept
{
    if (!aRequest)
        return;
    GetProperty<std::remove_cvref_t<T>>(aRequest.instance, aProp) = std::forward<T>(aValue);
}

void ApplyAttribute(Handle<game::ScriptableSystem>& aSystem, const Handle<game::Object>& aOwner,
                    const CapturedAttribute& aAttr) noexcept
{
    auto req = MakeRequest("SetAttribute");
    if (!req)
        return;
    SetReqProp(req, "owner", WeakHandle<game::Object>{aOwner});
    SetReqProp(req, "m_statLevel", static_cast<float>(aAttr.m_value));
    SetReqProp(req, "m_attributeType", aAttr.m_type);
    shared::raw::ScriptableSystem::QueueRequest(aSystem.instance, req);
}

void ApplyNewPerk(Handle<game::ScriptableSystem>& aSystem, const Handle<game::Object>& aOwner,
                  const CapturedNewPerk& aPerk) noexcept
{
    // BuyNewPerk extends NewPerkActionRequest; the `forceBuy` semantics in the plan map
    // to issuing one BuyNewPerk per level. The handler is OnBuyNewPerk → BuyNewPerk(owner, type)
    // which goes through PDS::BuyNewPerk; on script side that takes opt `forceBuy`, but the
    // request handler in playerDevelopmentSystem.swift calls the variant without forceBuy.
    // Verified-against-scripts note: re-issuing N times still levels up to N (BuyNewPerk
    // increments the level), but prerequisite gating may bite — apply attributes first
    // (caller orders this) and accept that a perk may be skipped if its tree is locked.
    for (std::int32_t i = 0; i < aPerk.m_level; ++i)
    {
        auto req = MakeRequest("BuyNewPerk");
        if (!req)
            return;
        SetReqProp(req, "owner", WeakHandle<game::Object>{aOwner});
        SetReqProp(req, "m_perkType", aPerk.m_type);
        shared::raw::ScriptableSystem::QueueRequest(aSystem.instance, req);
    }
}

void ApplyProficiency(Handle<game::ScriptableSystem>& aSystem, const Handle<game::Object>& aOwner,
                      const CapturedProficiency& aProf) noexcept
{
    auto req = MakeRequest("SetProficiencyLevel");
    if (!req)
        return;
    SetReqProp(req, "owner", WeakHandle<game::Object>{aOwner});
    SetReqProp(req, "m_newLevel", aProf.m_level);
    SetReqProp(req, "m_proficiencyType", aProf.m_type);
    // telemetryLevelGainReason.Ignore — the safe default for a non-gameplay grant.
    constexpr std::int32_t kIgnore = 2; // see telemetryLevelGainReason in orphans.swift
    SetReqProp(req, "m_telemetryLevelGainReason", kIgnore);
    shared::raw::ScriptableSystem::QueueRequest(aSystem.instance, req);
}

void ApplyTrait(Handle<game::ScriptableSystem>& aSystem, const Handle<game::Object>& aOwner,
                const CapturedTrait& aTrait) noexcept
{
    for (std::int32_t i = 0; i < aTrait.m_level; ++i)
    {
        auto req = MakeRequest("IncreaseTraitLevel");
        if (!req)
            return;
        SetReqProp(req, "owner", WeakHandle<game::Object>{aOwner});
        SetReqProp(req, "m_trait", aTrait.m_type);
        shared::raw::ScriptableSystem::QueueRequest(aSystem.instance, req);
    }
}

void ApplyDevPoints(Handle<game::ScriptableSystem>& aSystem, const Handle<game::Object>& aOwner,
                    const CapturedDevPoints& aPoints) noexcept
{
    // AddDevelopmentPointsRequest IS a native SDK type — use the typed handle.
    auto req = MakeHandle<quest::AddDevelopmentPointsRequest>();
    req->owner = aOwner;
    req->amountOfPoints = aPoints.m_unspent;
    req->developmentPointType = aPoints.m_type;
    Handle<game::ScriptableSystemRequest> base = req;
    shared::raw::ScriptableSystem::QueueRequest(aSystem.instance, base);
}
} // namespace

replay::PlayerProgressionSnapshot replay::PlayerProgression::Capture() noexcept
{
    PlayerProgressionSnapshot snapshot{};

    auto playerSystem = GetGameSystem<cp::PlayerSystem>();
    auto container = GetGameSystem<game::ScriptableSystemsContainer>();
    if (!playerSystem || !container)
    {
        replay::log::Warn("PlayerProgression::Capture: missing PlayerSystem/Container");
        return snapshot;
    }

    auto pds = GetPlayerDevelopmentSystem(container);
    if (!pds)
    {
        replay::log::Warn("PlayerProgression::Capture: PDS not found");
        return snapshot;
    }

    CaptureAttributes(pds, snapshot);
    CaptureNewPerks(pds, snapshot);
    CaptureProficiencies(pds, snapshot);
    CaptureTraits(pds, snapshot);
    CaptureDevPoints(pds, snapshot);

    snapshot.m_present = true;
    return snapshot;
}

void replay::PlayerProgression::Apply(const PlayerProgressionSnapshot& aSnapshot, cp::PlayerSystem* aPlayerSystem,
                                      game::ScriptableSystemsContainer* aScriptableContainer) noexcept
{
    if (!aSnapshot.m_present)
        return;

    auto playerObject = GetPlayerObject(aPlayerSystem);
    if (!playerObject)
    {
        replay::log::Warn("PlayerProgression::Apply: no player object");
        return;
    }

    auto pds = GetPlayerDevelopmentSystem(aScriptableContainer);
    if (!pds)
    {
        replay::log::Warn("PlayerProgression::Apply: PDS not found");
        return;
    }

    // Attributes first so perk prereqs see the final attribute floor.
    for (const auto& attr : aSnapshot.m_attributes)
        ApplyAttribute(pds, playerObject, attr);

    for (const auto& perk : aSnapshot.m_newPerks)
        ApplyNewPerk(pds, playerObject, perk);

    for (const auto& prof : aSnapshot.m_proficiencies)
        ApplyProficiency(pds, playerObject, prof);

    for (const auto& trait : aSnapshot.m_traits)
        ApplyTrait(pds, playerObject, trait);

    for (const auto& points : aSnapshot.m_devPoints)
        ApplyDevPoints(pds, playerObject, points);
}
