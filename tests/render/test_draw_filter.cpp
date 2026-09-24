#include "doctest.h"

#include "Render/Renderer/DrawFilter.h"

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

    CHECK(filter.Matches({42u, 12778u, 16u, 16u, false}));
    CHECK_FALSE(filter.Matches({42u, 12778u, 16u, 16u, true}));
    CHECK_FALSE(filter.Matches({42u, 12778u, 32u, 16u, false}));
    CHECK_FALSE(filter.Matches({42u, 3u, 16u, 16u, false}));
}

TEST_CASE("disabled draw filter preserves every draw [render][draw-filter]")
{
    Render::DrawFilter filter{};
    filter.hasTextureId = true;
    filter.textureId = 12778u;

    CHECK_FALSE(filter.Matches({0u, 12778u, 16u, 16u, false}));
}
