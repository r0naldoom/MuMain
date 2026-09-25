#pragma once

#include "DrawDiagnostics.h"

#include <array>

namespace Render::Diagnostic
{

// Adds the visible portion of one post-vertex triangle to a scope's bounds.
// Coordinates are homogeneous clip coordinates; width and height are physical
// swapchain pixels with a top-left origin.
void AddProjectedTriangle(mu::DrawDiagnosticScope& scope,
                          const std::array<float, 4>& first,
                          const std::array<float, 4>& second,
                          const std::array<float, 4>& third,
                          std::uint32_t viewportWidth,
                          std::uint32_t viewportHeight);

} // namespace Render::Diagnostic
