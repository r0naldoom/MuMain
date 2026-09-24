#pragma once

#include <cstdint>

namespace Render
{
struct DrawMetadata
{
    std::uint32_t submittedOrdinal = 0;
    std::uint32_t textureId = 0;
    std::uint32_t textureWidth = 0;
    std::uint32_t textureHeight = 0;
    bool blendEnabled = false;
};

struct DrawFilter
{
    bool enabled = false;
    bool hasSubmittedOrdinal = false;
    bool hasTextureId = false;
    bool hasTextureSize = false;
    bool hasBlend = false;
    std::uint32_t firstSubmittedOrdinal = 0;
    std::uint32_t lastSubmittedOrdinal = 0;
    std::uint32_t textureId = 0;
    std::uint32_t textureWidth = 0;
    std::uint32_t textureHeight = 0;
    bool blendEnabled = false;

    [[nodiscard]] bool Matches(const DrawMetadata& draw) const;
};
} // namespace Render
