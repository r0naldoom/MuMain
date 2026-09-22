#pragma once

#include <optional>
#include <string_view>

class CameraState;

namespace SceneFixture
{
struct TargetWindowSize
{
    unsigned int width;
    unsigned int height;
};

void ConfigureFromCommandLine(std::wstring_view commandLine);

[[nodiscard]] bool IsActive();
[[nodiscard]] bool ObserveServerSpawn(int map, unsigned char positionX, unsigned char positionY);
[[nodiscard]] std::optional<TargetWindowSize> GetTargetWindowSize();
[[nodiscard]] std::optional<float> GetWorldViewportBottomReserve();
[[nodiscard]] bool IsReady();
[[nodiscard]] const wchar_t* GetId();

void ApplyWorldTime(double& worldTime);
[[nodiscard]] bool ApplyCameraPose(CameraState& camera);
[[nodiscard]] float GetFixedStaticObjectLuminosity();
} // namespace SceneFixture
