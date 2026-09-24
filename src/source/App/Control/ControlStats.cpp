#include "App/Control/ControlStats.h"

#include "Render/Renderer/MuRenderer.h"

#include <cstdint>
#include <string_view>

namespace
{
void AppendField(std::string& result, bool& first, std::string_view name, std::uint32_t value)
{
    if (!first)
    {
        result += ',';
    }
    result += '"';
    result += name;
    result += "\":";
    result += std::to_string(value);
    first = false;
}
} // namespace

namespace App::Control::Stats
{
std::string ResultObject(const mu::RendererStats& stats)
{
    std::string result;
    result.reserve(768);
    result += '{';
    bool first = true;

    AppendField(result, first, "frame", stats.frame);
    AppendField(result, first, "s_dbgDrawCallsThisFrame", stats.requestedDrawCalls);
    AppendField(result, first, "s_dbgGpuDrawCallsThisFrame", stats.submittedDrawCalls);
    AppendField(result, first, "s_dbgRenderCmdsReplayedThisFrame", stats.renderCommandsReplayed);
    AppendField(result, first, "s_dbgFallbackTextureThisFrame", stats.fallbackTextureDraws);
    AppendField(result, first, "s_dbgWhiteTextureDrawsThisFrame", stats.whiteTextureDraws);
    AppendField(result, first, "s_dbgRealTextureDrawsThisFrame", stats.realTextureDraws);
    AppendField(result, first, "s_dbgMergedDrawsThisFrame", stats.mergedDrawCalls);
    AppendField(result, first, "s_dbgPipelineBindsThisFrame", stats.pipelineBinds);
    AppendField(result, first, "s_dbgSamplerBindsThisFrame", stats.samplerBinds);
    AppendField(result, first, "s_dbgFragmentUniformPushesThisFrame", stats.fragmentUniformPushes);
    AppendField(result, first, "s_dbgVtxBytesThisFrame", stats.vertexBytes);
    AppendField(result, first, "BatchBreakBlend", stats.batchBreakBlend);
    AppendField(result, first, "BatchBreakDepth", stats.batchBreakDepth);
    AppendField(result, first, "BatchBreakMatrix", stats.batchBreakMatrix);
    AppendField(result, first, "BatchBreakTexture", stats.batchBreakTexture);
    AppendField(result, first, "BatchBreakProgram", stats.batchBreakProgram);
    AppendField(result, first, "BatchBreakUniform", stats.batchBreakUniform);
    AppendField(result, first, "BatchBreakDraw", stats.batchBreakDraw);
    AppendField(result, first, "BatchBreakOther", stats.batchBreakOther);
    AppendField(result, first, "TextureUploads", stats.frameProfilerTextureUploads);

    result += '}';
    return result;
}
} // namespace App::Control::Stats
