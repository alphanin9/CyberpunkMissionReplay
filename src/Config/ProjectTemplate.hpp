#include <semver.hpp>

namespace Build 
{
constexpr auto Version = semver::from_string_noexcept("0.0.0").value();
}