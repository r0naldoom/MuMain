#include "stdafx.h"
#include "SceneFixture.h"


namespace
{
struct FixtureDefinition
{
    std::wstring_view id;
    SceneFixture::TargetWindowSize targetWindowSize;
    float worldViewportBottomReserve;
    int map;
    unsigned char positionX;
    unsigned char positionY;
    double worldTime;
    unsigned int captureAfterReadyFrames;
    unsigned int exitAfterCaptureFrames;
};

constexpr unsigned int LOST_TOWER_WALL_WARMUP_FRAMES = 240;
constexpr unsigned int LOST_TOWER_WALL_POST_CAPTURE_FRAMES = 2;

constexpr FixtureDefinition LOST_TOWER_WALL_V1 = {
    L"lost-tower-wall-v1",
    {800, 600},
    48.0f,
    4,
    // Inside the Lost Tower safe zone (terrain attribute 0x01 covers x 198..213, y 70..75): monsters neither enter
    // nor attack there, so no combat effect can add dynamic light to the captured terrain and static objects. The
    // south (y 69) and east (x 214..215) walls meet at (214, 69), so the default camera frames that wall corner.
    //
    // This exact tile was picked by driving the client over the control socket and reading `state` and `screenshot`
    // at each candidate: the wall runs diagonally across the frame here with the crystal-capped pillars in view,
    // and over 36 s of sampling only one monster stayed in `nearby`, wandering 9 to 11 tiles away. The neighbouring
    // tiles are worse: (213, 72) sits on a wander path and caught a monster in melee range with its aura in frame,
    // while (211, 71) and (208, 72) report five monsters and frame mostly floor.
    213,
    74,
    0.0,
    LOST_TOWER_WALL_WARMUP_FRAMES,
    LOST_TOWER_WALL_POST_CAPTURE_FRAMES,
};

enum class CaptureStage
{
    Idle,
    Waiting,
    Triggered,
    Settling,
    Exiting,
};

bool s_active = false;
bool s_ready = false;
bool s_captureWhenReady = false;
bool s_exitAfterCapture = false;
CaptureStage s_captureStage = CaptureStage::Idle;
unsigned int s_readyFrameCount = 0;
unsigned int s_framesAfterCapture = 0;

[[nodiscard]] bool HasLostTowerWallSelector(std::wstring_view commandLine)
{
    constexpr std::wstring_view prefix = L"--scene=";
    constexpr std::wstring_view whitespace = L" \t\r\n";
    std::size_t start = 0;

    while (start < commandLine.size())
    {
        const std::size_t end = commandLine.find_first_of(whitespace, start);
        const std::wstring_view argument = commandLine.substr(start, end - start);
        if (argument.starts_with(prefix) && argument.substr(prefix.size()) == LOST_TOWER_WALL_V1.id)
        {
            return true;
        }

        if (end == std::wstring_view::npos)
        {
            break;
        }
        start = commandLine.find_first_not_of(whitespace, end);
    }

    return false;
}

[[nodiscard]] bool HasCommandLineArgument(std::wstring_view commandLine, std::wstring_view expected)
{
    constexpr std::wstring_view whitespace = L" \t\r\n";
    std::size_t start = 0;

    while (start < commandLine.size())
    {
        const std::size_t end = commandLine.find_first_of(whitespace, start);
        if (commandLine.substr(start, end - start) == expected)
        {
            return true;
        }

        if (end == std::wstring_view::npos)
        {
            break;
        }
        start = commandLine.find_first_not_of(whitespace, end);
    }

    return false;
}
} // namespace

void SceneFixture::ConfigureFromCommandLine(std::wstring_view commandLine)
{
    s_active = HasLostTowerWallSelector(commandLine);
    s_ready = false;
    s_captureWhenReady = s_active && HasCommandLineArgument(commandLine, L"--capture-when-ready");
    s_exitAfterCapture = s_captureWhenReady && HasCommandLineArgument(commandLine, L"--exit-after-capture");
    s_captureStage = s_captureWhenReady ? CaptureStage::Waiting : CaptureStage::Idle;
    s_readyFrameCount = 0;
    s_framesAfterCapture = 0;
}

bool SceneFixture::IsActive()
{
    return s_active;
}

std::optional<SceneFixture::TargetWindowSize> SceneFixture::GetTargetWindowSize()
{
    if (!s_active)
    {
        return std::nullopt;
    }

    return LOST_TOWER_WALL_V1.targetWindowSize;
}

std::optional<float> SceneFixture::GetWorldViewportBottomReserve()
{
    if (!s_active)
    {
        return std::nullopt;
    }

    return LOST_TOWER_WALL_V1.worldViewportBottomReserve;
}

bool SceneFixture::ObserveServerSpawn(int map, unsigned char positionX, unsigned char positionY)
{
    if (!s_active)
    {
        return false;
    }

    const bool ready = map == LOST_TOWER_WALL_V1.map && positionX == LOST_TOWER_WALL_V1.positionX &&
                       positionY == LOST_TOWER_WALL_V1.positionY;
    if (ready && !s_ready && s_captureStage == CaptureStage::Waiting)
    {
        s_readyFrameCount = 0;
    }

    s_ready = ready;
    return s_ready;
}


bool SceneFixture::ShouldTriggerCaptureForFrame()
{
    if (!s_ready || s_captureStage != CaptureStage::Waiting)
    {
        return false;
    }

    if (s_readyFrameCount < LOST_TOWER_WALL_V1.captureAfterReadyFrames)
    {
        ++s_readyFrameCount;
        return false;
    }

    s_captureStage = CaptureStage::Triggered;
    return true;
}

void SceneFixture::NotifyCaptureTriggered()
{
    s_framesAfterCapture = 0;
}

void SceneFixture::NotifyCaptureSkipped()
{
    s_captureStage = CaptureStage::Idle;
}

bool SceneFixture::ShouldExitAfterCapturedFrame()
{
    if (!s_exitAfterCapture)
    {
        return false;
    }

    if (s_captureStage == CaptureStage::Triggered)
    {
        s_captureStage = CaptureStage::Settling;
        return false;
    }

    if (s_captureStage != CaptureStage::Settling)
    {
        return false;
    }

    if (s_framesAfterCapture < LOST_TOWER_WALL_V1.exitAfterCaptureFrames)
    {
        ++s_framesAfterCapture;
        return false;
    }

    s_captureStage = CaptureStage::Exiting;
    return true;
}

const wchar_t* SceneFixture::GetId()
{
    return LOST_TOWER_WALL_V1.id.data();
}

void SceneFixture::ApplyWorldTime(double& worldTime)
{
    if (s_active)
    {
        worldTime = LOST_TOWER_WALL_V1.worldTime;
    }
}
