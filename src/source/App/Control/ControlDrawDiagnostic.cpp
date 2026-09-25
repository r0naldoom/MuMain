#include "stdafx.h"
#include "App/Control/ControlDrawDiagnostic.h"

#include "Render/Renderer/MuRenderer.h"

#include "json.hpp"

namespace
{

using App::Control::EncodeError;
using App::Control::EncodeResult;
using App::Control::ErrorCode;
using App::Control::Request;
using json = nlohmann::json;

constexpr std::string_view GroundItemCategory = "ground_item";

json Position(const float* value)
{
    return {value[0], value[1], value[2]};
}

json ScopeObject(const mu::DrawDiagnosticScope& scope)
{
    json result;
    result["scope"] = scope.ordinal;
    result["geometry_commands"] = scope.geometryCommands;
    result["submitted_commands"] = scope.submittedCommands;
    result["dropped_commands"] = scope.droppedCommands;
    result["filtered_commands"] = scope.filteredCommands;
    result["projected_triangles"] = scope.projectedTriangles;
    if (scope.hasSourceOrigin)
    {
        result["source_origin"] = Position(scope.sourceOrigin);
    }
    if (scope.hasEffectiveSkinningOrigin)
    {
        result["effective_skinning_origin"] = Position(scope.effectiveSkinningOrigin);
        result["effective_skinning_scale"] = scope.effectiveSkinningScale;
    }
    if (scope.hasProjectedBounds)
    {
        result["projected_bbox"] = {scope.projectedBounds[0], scope.projectedBounds[1],
                                    scope.projectedBounds[2], scope.projectedBounds[3]};
    }
    return result;
}

} // namespace

namespace App::Control::Diagnostics
{

std::string Draw(const Request& request)
{
    std::string category;
    if (!request.GetString("category", category) || category != GroundItemCategory)
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "draw-diagnostic needs category ground_item");
    }

    int requestedFrame = 0;
    if (request.Has("frame") && (!request.GetInt("frame", requestedFrame) || requestedFrame < 1))
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "frame must be a positive integer");
    }

    const mu::DrawDiagnosticSnapshot snapshot =
        mu::GetRenderer().GetDrawDiagnosticSnapshot(static_cast<std::uint32_t>(requestedFrame));
    if (snapshot.label != mu::RenderDebugLabel::GroundItem)
    {
        return EncodeError(request.EncodedId(), ErrorCode::Failed,
                           "no complete ground_item isolation frame is available");
    }

    json result;
    result["frame"] = snapshot.frame;
    result["category"] = GroundItemCategory;
    result["viewport"] = {snapshot.viewportWidth, snapshot.viewportHeight};
    result["scopes"] = json::array();
    for (const mu::DrawDiagnosticScope& scope : snapshot.scopes)
    {
        result["scopes"].push_back(ScopeObject(scope));
    }
    return EncodeResult(request.EncodedId(), result.dump());
}

} // namespace App::Control::Diagnostics
