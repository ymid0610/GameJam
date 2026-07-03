#pragma once

#include "stdafx.h"

namespace TowerDefenseRoute
{
    inline constexpr int RouteCount = 3;
    inline constexpr float StartX = 0.03f;
    inline constexpr float MeetX = 0.50f;
    inline constexpr float EndX = 0.97f;
    inline constexpr float MeetZ = 0.50f;

    inline float Smooth01(float value)
    {
        value = clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - value * 2.0f);
    }

    inline float BranchStartZ(int routeIndex)
    {
        static constexpr float Starts[RouteCount] = { 0.22f, 0.50f, 0.78f };
        return Starts[clamp(routeIndex, 0, RouteCount - 1)];
    }

    inline float SharedZ(float t)
    {
        t = clamp(t, 0.0f, 1.0f);
        return MeetZ + sinf(t * XM_PI * 1.05f) * 0.06f;
    }

    inline float CenterZ(int routeIndex, float normalizedX)
    {
        if (normalizedX <= MeetX)
        {
            const float t = (normalizedX - StartX) / (MeetX - StartX);
            const float startZ = BranchStartZ(routeIndex);
            return startZ + (MeetZ - startZ) * Smooth01(t);
        }

        const float t = (normalizedX - MeetX) / (EndX - MeetX);
        return SharedZ(t);
    }
}
