#include "doctest.h"

#include "Render/Renderer/DrawFilter.h"
#include "Render/Renderer/MuRenderer.h"

TEST_CASE("draw filter requires every supplied clause [render][draw-filter]")
{
    Render::DrawFilter filter{};
    filter.enabled = true;
    filter.hasTextureId = true;
    filter.textureId = 12778u;
    filter.hasTextureSize = true;
    filter.textureWidth = 16u;
    filter.textureHeight = 16u;
    filter.hasBlend = true;
    filter.blendEnabled = false;

    CHECK(filter.Matches({42u, 12778u, 16u, 16u, 0u, false}));
    CHECK_FALSE(filter.Matches({42u, 12778u, 16u, 16u, 0u, true}));
    CHECK_FALSE(filter.Matches({42u, 12778u, 32u, 16u, 0u, false}));
    CHECK_FALSE(filter.Matches({42u, 3u, 16u, 16u, 0u, false}));
}

TEST_CASE("disabled draw filter preserves every draw [render][draw-filter]")
{
    Render::DrawFilter filter{};
    filter.hasTextureId = true;
    filter.textureId = 12778u;

    CHECK_FALSE(filter.Matches({0u, 12778u, 16u, 16u, 0u, false}));
}

TEST_CASE("draw filter can replay only matching weapon effects [render][draw-filter]")
{
    const auto groundItem = static_cast<std::uint8_t>(mu::RenderDebugLabel::GroundItem);
    const auto weaponEffect = static_cast<std::uint8_t>(mu::RenderDebugLabel::WeaponEffect);
    Render::DrawFilter filter{};
    filter.enabled = true;
    filter.onlyMatches = true;
    filter.hasDebugLabel = true;
    filter.debugLabel = weaponEffect;

    CHECK_FALSE(filter.Suppresses({42u, 12778u, 16u, 16u, weaponEffect, false}));
    CHECK(filter.Suppresses({42u, 12778u, 16u, 16u, groundItem, false}));
}

TEST_CASE("draw filter can replay only matching characters [render][draw-filter]")
{
    const auto character = static_cast<std::uint8_t>(mu::RenderDebugLabel::Character);
    const auto weaponEffect = static_cast<std::uint8_t>(mu::RenderDebugLabel::WeaponEffect);
    Render::DrawFilter filter{};
    filter.enabled = true;
    filter.onlyMatches = true;
    filter.hasDebugLabel = true;
    filter.debugLabel = character;

    CHECK_FALSE(filter.Suppresses({42u, 12778u, 16u, 16u, character, false}));
    CHECK(filter.Suppresses({42u, 12778u, 16u, 16u, weaponEffect, false}));
}
