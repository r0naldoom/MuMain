#include "DrawDiagnosticProjection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{

constexpr std::size_t MaxClippedVertices = 10;
constexpr float MinimumW = 0.000001f;

struct ClipPlane
{
    std::array<float, 4> coefficients;
};

constexpr std::array ClipPlanes{
    ClipPlane{{1.0f, 0.0f, 0.0f, 1.0f}},
    ClipPlane{{-1.0f, 0.0f, 0.0f, 1.0f}},
    ClipPlane{{0.0f, 1.0f, 0.0f, 1.0f}},
    ClipPlane{{0.0f, -1.0f, 0.0f, 1.0f}},
    ClipPlane{{0.0f, 0.0f, 1.0f, 0.0f}},
    ClipPlane{{0.0f, 0.0f, -1.0f, 1.0f}},
    ClipPlane{{0.0f, 0.0f, 0.0f, 1.0f - MinimumW}},
};

float Distance(const std::array<float, 4>& vertex, const ClipPlane& plane)
{
    return vertex[0] * plane.coefficients[0] + vertex[1] * plane.coefficients[1] +
           vertex[2] * plane.coefficients[2] + vertex[3] * plane.coefficients[3];
}

std::array<float, 4> Intersection(const std::array<float, 4>& first,
                                  const std::array<float, 4>& second,
                                  float firstDistance,
                                  float secondDistance)
{
    const float fraction = firstDistance / (firstDistance - secondDistance);
    return {
        first[0] + (second[0] - first[0]) * fraction,
        first[1] + (second[1] - first[1]) * fraction,
        first[2] + (second[2] - first[2]) * fraction,
        first[3] + (second[3] - first[3]) * fraction,
    };
}

void AddPoint(mu::DrawDiagnosticScope& scope, const std::array<float, 4>& vertex, float width, float height)
{
    const float inverseW = 1.0f / vertex[3];
    const float x = (vertex[0] * inverseW + 1.0f) * width * 0.5f;
    const float y = (1.0f - vertex[1] * inverseW) * height * 0.5f;
    if (!std::isfinite(x) || !std::isfinite(y))
    {
        return;
    }

    if (!scope.hasProjectedBounds)
    {
        scope.projectedBounds[0] = x;
        scope.projectedBounds[1] = y;
        scope.projectedBounds[2] = x;
        scope.projectedBounds[3] = y;
        scope.hasProjectedBounds = true;
        return;
    }

    scope.projectedBounds[0] = std::min(scope.projectedBounds[0], x);
    scope.projectedBounds[1] = std::min(scope.projectedBounds[1], y);
    scope.projectedBounds[2] = std::max(scope.projectedBounds[2], x);
    scope.projectedBounds[3] = std::max(scope.projectedBounds[3], y);
}

} // namespace

namespace Render::Diagnostic
{

void AddProjectedTriangle(mu::DrawDiagnosticScope& scope,
                          const std::array<float, 4>& first,
                          const std::array<float, 4>& second,
                          const std::array<float, 4>& third,
                          std::uint32_t viewportWidth,
                          std::uint32_t viewportHeight)
{
    if (viewportWidth == 0 || viewportHeight == 0)
    {
        return;
    }

    std::array<std::array<float, 4>, MaxClippedVertices> input{first, second, third};
    std::size_t inputCount = 3;
    for (const ClipPlane& plane : ClipPlanes)
    {
        std::array<std::array<float, 4>, MaxClippedVertices> output{};
        std::size_t outputCount = 0;
        for (std::size_t index = 0; index < inputCount; ++index)
        {
            const std::array<float, 4>& current = input[index];
            const std::array<float, 4>& previous = input[(index + inputCount - 1) % inputCount];
            const float currentDistance = Distance(current, plane);
            const float previousDistance = Distance(previous, plane);
            const bool currentInside = currentDistance >= 0.0f;
            const bool previousInside = previousDistance >= 0.0f;
            if (currentInside != previousInside && outputCount < output.size())
            {
                output[outputCount++] = Intersection(previous, current, previousDistance, currentDistance);
            }
            if (currentInside && outputCount < output.size())
            {
                output[outputCount++] = current;
            }
        }
        if (outputCount == 0)
        {
            return;
        }
        input = output;
        inputCount = outputCount;
    }

    ++scope.projectedTriangles;
    const float width = static_cast<float>(viewportWidth);
    const float height = static_cast<float>(viewportHeight);
    for (std::size_t index = 0; index < inputCount; ++index)
    {
        AddPoint(scope, input[index], width, height);
    }
}

} // namespace Render::Diagnostic
