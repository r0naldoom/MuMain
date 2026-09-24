#include "stdafx.h"
#include "SceneFixture.h"

#include <array>
#include <cstdlib>


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
    unsigned int randomSeed;
    unsigned int captureAfterReadyFrames;
    unsigned int exitAfterCaptureFrames;
};

// The client seeds the RNG from the clock, and object lighting reads it while the map loads: the crystal caps on
// the Lost Tower pillars come out at a different brightness in every run. Two captures of the same build differed
// on 0.31% of the frame, in ten blobs sitting exactly on the two pillar rows, one of them swinging a channel by 188
// of 255. A fixed seed removes that whole class of noise from a parity run; both builds must use the same value,
// which is why every scene here shares this one.
constexpr unsigned int PARITY_RANDOM_SEED = 20260924u;
constexpr unsigned int WARMUP_FRAMES = 240;
constexpr unsigned int POST_CAPTURE_FRAMES = 2;

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
    PARITY_RANDOM_SEED,
    WARMUP_FRAMES,
    POST_CAPTURE_FRAMES,
};

// A hero close-up for the wing effects, on the same Lost Tower tile as the wall scene. Noria's grass looked like
// the natural stage for it and was tried first, but (139, 108) has a Goblin parked on (138, 108) that the scene
// gate rejected on every attempt, so this reuses the stage whose noise floor is already measured at 0.000000 on
// both builds: anything that differs outside the wings here is a red flag about the run, not about the wings.
//
// The world time is what makes this a separate scene. The wing flares read it directly:
// RenderPartObjectEffect's MODEL_WING_OF_ILLUSION branch derives both the sprite scale and the emitted colour from
// absf(sinf(WorldTime * 0.002f)), a 1.57 s pulse. A capture taken at an arbitrary phase compares two different
// brightnesses, so a single pair would prove nothing. Pinning the time to 785.398 ms puts the pulse exactly on its
// crest (sinf(1.5708) == 1), which is both deterministic and the worst case for additive saturation: the flares
// are at their largest and brightest, so any difference in how the two builds accumulate them shows here first.
constexpr FixtureDefinition HERO_WINGS_V1 = {
    L"hero-wings-v1",
    {800, 600},
    48.0f,
    4,
    213,
    74,
    785.398163397448,
    PARITY_RANDOM_SEED,
    WARMUP_FRAMES,
    POST_CAPTURE_FRAMES,
};

constexpr std::array<const FixtureDefinition*, 2> DEFINITIONS = {&LOST_TOWER_WALL_V1, &HERO_WINGS_V1};

enum class CaptureStage
{
    Idle,
    Waiting,
    Triggered,
    Settling,
    Exiting,
};

const FixtureDefinition* s_definition = nullptr;
bool s_ready = false;
bool s_captureWhenReady = false;
bool s_exitAfterCapture = false;
CaptureStage s_captureStage = CaptureStage::Idle;
unsigned int s_readyFrameCount = 0;
unsigned int s_framesAfterCapture = 0;

[[nodiscard]] const FixtureDefinition* FindSceneSelector(std::wstring_view commandLine)
{
    constexpr std::wstring_view prefix = L"--scene=";
    constexpr std::wstring_view whitespace = L" \t\r\n";
    std::size_t start = 0;

    while (start < commandLine.size())
    {
        const std::size_t end = commandLine.find_first_of(whitespace, start);
        const std::wstring_view argument = commandLine.substr(start, end - start);
        if (argument.starts_with(prefix))
        {
            const std::wstring_view id = argument.substr(prefix.size());
            for (const FixtureDefinition* definition : DEFINITIONS)
            {
                if (definition->id == id)
                {
                    return definition;
                }
            }
        }

        if (end == std::wstring_view::npos)
        {
            break;
        }
        start = commandLine.find_first_not_of(whitespace, end);
    }

    return nullptr;
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
    s_definition = FindSceneSelector(commandLine);
    s_ready = false;
    s_captureWhenReady = s_definition != nullptr && HasCommandLineArgument(commandLine, L"--capture-when-ready");
    s_exitAfterCapture = s_captureWhenReady && HasCommandLineArgument(commandLine, L"--exit-after-capture");
    s_captureStage = s_captureWhenReady ? CaptureStage::Waiting : CaptureStage::Idle;
    s_readyFrameCount = 0;
    s_framesAfterCapture = 0;
}

bool SceneFixture::IsActive()
{
    return s_definition != nullptr;
}

std::optional<SceneFixture::TargetWindowSize> SceneFixture::GetTargetWindowSize()
{
    if (s_definition == nullptr)
    {
        return std::nullopt;
    }

    return s_definition->targetWindowSize;
}

std::optional<float> SceneFixture::GetWorldViewportBottomReserve()
{
    if (s_definition == nullptr)
    {
        return std::nullopt;
    }

    return s_definition->worldViewportBottomReserve;
}

bool SceneFixture::ObserveServerSpawn(int map, unsigned char positionX, unsigned char positionY)
{
    if (s_definition == nullptr)
    {
        return false;
    }

    const bool ready =
        map == s_definition->map && positionX == s_definition->positionX && positionY == s_definition->positionY;
    if (ready && !s_ready)
    {
        // Seeding at startup is not enough: the number of frames spent on the login and character scenes varies,
        // and every rand() consumed there shifts the stream. Re-seeding the moment the fixture becomes ready makes
        // the frames that follow consume the same values in every run, on both builds.
        srand(s_definition->randomSeed);
    }

    if (ready && !s_ready && s_captureStage == CaptureStage::Waiting)
    {
        s_readyFrameCount = 0;
    }

    s_ready = ready;
    return s_ready;
}


std::optional<unsigned int> SceneFixture::GetRandomSeed()
{
    if (s_definition == nullptr)
    {
        return std::nullopt;
    }

    return s_definition->randomSeed;
}

bool SceneFixture::ShouldTriggerCaptureForFrame()
{
    if (!s_ready || s_captureStage != CaptureStage::Waiting)
    {
        return false;
    }

    if (s_readyFrameCount < s_definition->captureAfterReadyFrames)
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

    if (s_framesAfterCapture < s_definition->exitAfterCaptureFrames)
    {
        ++s_framesAfterCapture;
        return false;
    }

    s_captureStage = CaptureStage::Exiting;
    return true;
}

const wchar_t* SceneFixture::GetId()
{
    return s_definition == nullptr ? L"" : s_definition->id.data();
}

void SceneFixture::ApplyWorldTime(double& worldTime)
{
    if (s_definition != nullptr)
    {
        worldTime = s_definition->worldTime;
    }
}
