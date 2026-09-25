#include "App/Control/ConsoleCommand.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string_view>

namespace
{
#ifndef CSK_LH_DEBUG_CONSOLE
// Mirrors the CSK_LH_DEBUG_CONSOLE block of CmuConsoleDebug::CheckCommand, which is the source of
// truth. It buys nothing but a precise error: those branches are compiled out of Release anyway, so
// without this list the parser just answers false and the caller reads Unrecognized. A command added
// there and not here therefore degrades the error, not the behaviour -- but it does degrade it
// silently, so keep the two in step.
constexpr std::array<std::wstring_view, 12> DebugOnlyCommands = {
    L"$open",      L"$close",      L"$clear",   L"$type_test", L"$texture_info", L"$color_test",
    L"$mapatt on", L"$mapatt off", L"$path on", L"$path off",  L"$bb on",        L"$bb off",
};

bool IsDebugOnlyCommand(std::wstring_view command)
{
    return std::find(DebugOnlyCommands.begin(), DebugOnlyCommands.end(), command) != DebugOnlyCommands.end();
}
#endif
} // namespace

namespace App::Control::Console
{
Result Execute(const std::wstring& command, Parser parser)
{
#ifndef CSK_LH_DEBUG_CONSOLE
    if (IsDebugOnlyCommand(command))
    {
        return Result::DebugOnly;
    }
#endif

    try
    {
        return parser(command) ? Result::Consumed : Result::Unrecognized;
    }
    catch (const std::exception&)
    {
        return Result::Invalid;
    }
}
} // namespace App::Control::Console
