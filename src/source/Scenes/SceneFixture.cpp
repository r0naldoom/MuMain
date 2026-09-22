#include "stdafx.h"
#include "SceneFixture.h"

#include "Camera/CameraState.h"

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
    float cameraPositionX;
    float cameraPositionY;
    float cameraPositionZ;
    float cameraPitch;
    float cameraYaw;
    float cameraRoll;
    float cameraDistance;
    float staticObjectLuminosity;
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
    208,
    75,
    0.0,
    20800.0f,
    7500.0f,
    1300.0f,
    -48.5f,
    0.0f,
    -45.0f,
    1300.0f,
    0.7f,
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

bool SceneFixture::IsReady()
{
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

bool SceneFixture::ApplyCameraPose(CameraState& camera)
{
    if (!s_ready)
    {
        return false;
    }

    Vector(LOST_TOWER_WALL_V1.cameraPositionX, LOST_TOWER_WALL_V1.cameraPositionY, LOST_TOWER_WALL_V1.cameraPositionZ,
           camera.Position);
    Vector(LOST_TOWER_WALL_V1.cameraPitch, LOST_TOWER_WALL_V1.cameraYaw, LOST_TOWER_WALL_V1.cameraRoll, camera.Angle);
    camera.Distance = LOST_TOWER_WALL_V1.cameraDistance;
    camera.DistanceTarget = LOST_TOWER_WALL_V1.cameraDistance;
    camera.CustomDistance = 0.0f;
    camera.TopViewEnable = false;
    camera.UpdateMatrix();
    return true;
}

float SceneFixture::GetFixedStaticObjectLuminosity()
{
    return LOST_TOWER_WALL_V1.staticObjectLuminosity;
}
