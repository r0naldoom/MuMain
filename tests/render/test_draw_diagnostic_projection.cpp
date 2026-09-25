#include "doctest.h"

#include "Render/Renderer/DrawDiagnosticProjection.h"


TEST_CASE("projected diagnostic bounds use top-left viewport pixels [render][draw-diagnostic]")
{
    mu::DrawDiagnosticScope scope;

    Render::Diagnostic::AddProjectedTriangle(scope,
                                             {-0.5f, -0.5f, 0.5f, 1.0f},
                                             {0.5f, -0.5f, 0.5f, 1.0f},
                                             {0.0f, 0.5f, 0.5f, 1.0f},
                                             800u,
                                             600u);

    REQUIRE(scope.hasProjectedBounds);
    CHECK(scope.projectedTriangles == 1u);
    CHECK(scope.projectedBounds[0] == 200.0f);
    CHECK(scope.projectedBounds[1] == 150.0f);
    CHECK(scope.projectedBounds[2] == 600.0f);
    CHECK(scope.projectedBounds[3] == 450.0f);
}

TEST_CASE("clipped diagnostic triangles do not create offscreen bounds [render][draw-diagnostic]")
{
    mu::DrawDiagnosticScope scope;

    Render::Diagnostic::AddProjectedTriangle(scope,
                                             {2.0f, 0.0f, 0.5f, 1.0f},
                                             {3.0f, 1.0f, 0.5f, 1.0f},
                                             {3.0f, -1.0f, 0.5f, 1.0f},
                                             800u,
                                             600u);

    CHECK_FALSE(scope.hasProjectedBounds);
    CHECK(scope.projectedTriangles == 0u);
}
