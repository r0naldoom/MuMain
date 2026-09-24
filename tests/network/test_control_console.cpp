// doctest coverage for the control socket's local console-command adapter.
//
// Run: ctest --test-dir <build directory> --build-config Release -R "Control console"

#include "doctest.h"

#include "App/Control/ConsoleCommand.h"
#include "App/Control/ControlStats.h"
#include "Render/Renderer/MuRenderer.h"

#include <stdexcept>
#include <string>

namespace
{
bool ConsumeCommand(const std::wstring& command)
{
    return command == L"$effects sprites off";
}

bool RejectCommand(const std::wstring&)
{
    return false;
}

bool ThrowForMalformedCommand(const std::wstring&)
{
    throw std::invalid_argument("invalid argument");
}

bool parserCalled = false;

bool RecordParserCall(const std::wstring&)
{
    parserCalled = true;
    return true;
}
} // namespace

TEST_CASE("Control console passes recognized commands to its parser [network][control-console]")
{
    CHECK(App::Control::Console::Execute(L"$effects sprites off", ConsumeCommand) ==
          App::Control::Console::Result::Consumed);
}

TEST_CASE("Control console reports unrecognized parser input [network][control-console]")
{
    CHECK(App::Control::Console::Execute(L"$not-a-console-command", RejectCommand) ==
          App::Control::Console::Result::Unrecognized);
}

TEST_CASE("Control console contains malformed parser input [network][control-console]")
{
    CHECK(App::Control::Console::Execute(L"$fps invalid", ThrowForMalformedCommand) ==
          App::Control::Console::Result::Invalid);
}

#ifndef CSK_LH_DEBUG_CONSOLE
TEST_CASE("Control console identifies Debug-only commands in Release [network][control-console]")
{
    parserCalled = false;

    CHECK(App::Control::Console::Execute(L"$open", RecordParserCall) == App::Control::Console::Result::DebugOnly);
    CHECK_FALSE(parserCalled);
}
#endif

TEST_CASE("Control stats reports the completed renderer frame [network][control-console]")
{
    mu::RendererStats stats{};
    stats.frame = 417;
    stats.requestedDrawCalls = 230;
    stats.submittedDrawCalls = 91;
    stats.renderCommandsReplayed = 88;
    stats.fallbackTextureDraws = 3;
    stats.whiteTextureDraws = 5;
    stats.realTextureDraws = 225;
    stats.geometryCommands = 96;
    stats.droppedDraws = 2;
    stats.filteredDraws = 3;
    stats.mergedDrawCalls = 139;
    stats.pipelineBinds = 47;
    stats.samplerBinds = 44;
    stats.fragmentUniformPushes = 31;
    stats.vertexBytes = 4096;
    stats.batchBreakBlend = 7;
    stats.batchBreakDepth = 6;
    stats.batchBreakMatrix = 5;
    stats.batchBreakTexture = 4;
    stats.batchBreakProgram = 3;
    stats.batchBreakUniform = 2;
    stats.batchBreakDraw = 1;
    stats.batchBreakOther = 8;
    stats.frameProfilerTextureUploads = 9;

    CHECK(
        App::Control::Stats::ResultObject(stats) ==
        R"({"frame":417,"s_dbgDrawCallsThisFrame":230,"s_dbgGpuDrawCallsThisFrame":91,"s_dbgRenderCmdsReplayedThisFrame":88,"s_dbgFallbackTextureThisFrame":3,"s_dbgWhiteTextureDrawsThisFrame":5,"s_dbgRealTextureDrawsThisFrame":225,"s_dbgGeometryCmdsThisFrame":96,"s_dbgDroppedDrawsThisFrame":2,"s_dbgFilteredDrawsThisFrame":3,"s_dbgMergedDrawsThisFrame":139,"s_dbgPipelineBindsThisFrame":47,"s_dbgSamplerBindsThisFrame":44,"s_dbgFragmentUniformPushesThisFrame":31,"s_dbgVtxBytesThisFrame":4096,"BatchBreakBlend":7,"BatchBreakDepth":6,"BatchBreakMatrix":5,"BatchBreakTexture":4,"BatchBreakProgram":3,"BatchBreakUniform":2,"BatchBreakDraw":1,"BatchBreakOther":8,"TextureUploads":9})");
}
