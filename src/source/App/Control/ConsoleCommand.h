#pragma once

#include <cstdint>
#include <string>

namespace App::Control::Console
{
enum class Result : std::uint8_t
{
    Consumed,
    Unrecognized,
    Invalid,
    DebugOnly,
};

using Parser = bool (*)(const std::wstring& command);

[[nodiscard]] Result Execute(const std::wstring& command, Parser parser);
} // namespace App::Control::Console
