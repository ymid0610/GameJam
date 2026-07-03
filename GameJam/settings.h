#pragma once

namespace Settings
{
    constexpr UINT DefaultWindowWidth = 1920;
    constexpr UINT DefaultWindowHeight = 1080;

    constexpr FLOAT DefaultCameraPitch = XM_PIDIV2 - 0.3f;
    constexpr FLOAT DefaultCameraYaw = 0.f;
    constexpr FLOAT DefaultSpringArmYaw = -XM_PIDIV2;
    constexpr FLOAT DefaultCameraRadius = 10.f;
    constexpr FLOAT CameraMinPitch = XM_PIDIV2 - 0.9f;
    constexpr FLOAT CameraMaxPitch = XM_PIDIV2 + 0.9f;
    constexpr FLOAT FirstPersonEyeHeight = 1.2f;
    constexpr FLOAT FirstPersonMinPitch = -XM_PIDIV2 + 0.1f;
    constexpr FLOAT FirstPersonMaxPitch = XM_PIDIV2 - 0.1f;

    constexpr FLOAT CapsuleRadius = 0.5f;
    constexpr FLOAT CapsuleHeight = 1.0f;

    constexpr FLOAT PlayerSpeed = 5.f;
    constexpr FLOAT BulletSpeed = 38.0f;
    constexpr FLOAT BulletLifetime = 2.0f;
    constexpr FLOAT BulletRadius = 0.06f;
    constexpr size_t MaxBullets = 128;
    constexpr FLOAT DebugCameraSpeed = 14.0f;
    constexpr FLOAT DebugCameraFastMultiplier = 3.0f;
}
