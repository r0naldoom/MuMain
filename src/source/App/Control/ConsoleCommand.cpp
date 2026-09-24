#include "App/Control/ConsoleCommand.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string_view>

namespace
{
#ifndef CSK_LH_DEBUG_CONSOLE
constexpr std::array<std::wstring_view, 12> DebugOnlyCommands = {
    L"$open",         L"$close",          L"$clear",       L"$type_test",
    L"$texture_info", L"$color_test",     L"$mapatt on",   L"$mapatt off",
    L"$path on",      L"$path off",       L"$bb on",       L"$bb off",
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
