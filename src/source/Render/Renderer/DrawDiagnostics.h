#pragma once

#include <cstdint>
#include <vector>

namespace mu
{

enum class RenderDebugLabel : std::uint8_t
{
    None,
    StaticObjectsComplete,
    GroundItem,
    InventoryPreview,
    WeaponEffect,
    SharedBonePalette,
    Character,
};

struct DrawDiagnosticScope
{
    std::uint32_t ordinal = 0;
    RenderDebugLabel label = RenderDebugLabel::None;
    float sourceOrigin[3]{};
    bool hasSourceOrigin = false;
    std::uint32_t geometryCommands = 0;
    std::uint32_t submittedCommands = 0;
    std::uint32_t droppedCommands = 0;
    std::uint32_t filteredCommands = 0;
    std::uint32_t projectedTriangles = 0;
    float effectiveSkinningOrigin[3]{};
    float effectiveSkinningScale = 1.0f;
    bool hasEffectiveSkinningOrigin = false;
    float projectedBounds[4]{};
    bool hasProjectedBounds = false;
};

struct DrawDiagnosticSnapshot
{
    std::uint32_t frame = 0;
    RenderDebugLabel label = RenderDebugLabel::None;
    std::uint32_t viewportWidth = 0;
    std::uint32_t viewportHeight = 0;
    std::vector<DrawDiagnosticScope> scopes;
};

} // namespace mu
