#pragma once

#include <string>

namespace mu
{
struct RendererStats;
}

namespace App::Control::Stats
{
[[nodiscard]] std::string ResultObject(const mu::RendererStats& stats);
} // namespace App::Control::Stats
