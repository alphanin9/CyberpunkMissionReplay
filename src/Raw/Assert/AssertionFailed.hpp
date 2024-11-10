#pragma once
#include <Detail/AddressHashes.hpp>
#include <Util/Core.hpp>

#include <format>
#include <source_location>

namespace raw::Assert
{
constexpr auto RaiseAssertion = Util::RawFunc < UsedAddressHashes::Assert_RaiseAssert,
               void *(*)(const char* aSourceFile, int aSourceLineIndex, const char* aSourceLine,
                         const char* aAssertFmtStr)>();
template<typename... Args>
inline void RaiseAssert(const std::source_location& aLoc, std::string_view aFmtStr, Args&&... aArgs)
{
    auto str = std::vformat(aFmtStr, std::make_format_args(aArgs...));

    RaiseAssertion(aLoc.file_name(), aLoc.line(), aLoc.function_name(), str.c_str());

    __debugbreak();
}
}