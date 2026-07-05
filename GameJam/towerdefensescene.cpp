#include "towerdefensescene.h"
#include "framework.h"
#include "towerdefenseroute.h"

#include <cctype>
#include <cfloat>
#include <filesystem>
#include <fstream>

namespace
{
    enum class TowerInfoWidget : size_t
    {
        Panel,
        TowerIcon,
        DamageIcon,
        DamageTrack,
        DamageFill,
        RangeIcon,
        RangeTrack,
        RangeFill,
        FireRateIcon,
        FireRateTrack,
        FireRateFill,
        CooldownIcon,
        CooldownTrack,
        CooldownFill,
        Count
    };

    enum class GoldUiWidget : size_t
    {
        Panel,
        Coin,
        Life
    };

    enum class WaveUiWidget : size_t
    {
        Panel,
        Count
    };

    enum class BossHealthWidget : size_t
    {
        Panel,
        Track,
        Fill,
        Count
    };

    size_t WidgetIndex(TowerInfoWidget widget)
    {
        return static_cast<size_t>(widget);
    }

    size_t GoldWidgetIndex(GoldUiWidget widget)
    {
        return static_cast<size_t>(widget);
    }

    size_t WaveWidgetIndex(WaveUiWidget widget)
    {
        return static_cast<size_t>(widget);
    }

    size_t BossWidgetIndex(BossHealthWidget widget)
    {
        return static_cast<size_t>(widget);
    }

    int TowerTypeIndex(TowerDefenseTowerType type)
    {
        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            return 1;
        case TowerDefenseTowerType::Splash:
            return 2;
        case TowerDefenseTowerType::Slow:
            return 3;
        case TowerDefenseTowerType::Mortar:
            return 4;
        case TowerDefenseTowerType::Flak:
            return 5;
        default:
            return 0;
        }
    }

    TowerDefenseTowerType TowerTypeFromIndex(int index)
    {
        switch (clamp(index, 0, 5))
        {
        case 1:
            return TowerDefenseTowerType::Rapid;
        case 2:
            return TowerDefenseTowerType::Splash;
        case 3:
            return TowerDefenseTowerType::Slow;
        case 4:
            return TowerDefenseTowerType::Mortar;
        case 5:
            return TowerDefenseTowerType::Flak;
        default:
            return TowerDefenseTowerType::Basic;
        }
    }

    const wchar_t* TowerTypeShortName(TowerDefenseTowerType type)
    {
        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            return L"RAP";
        case TowerDefenseTowerType::Splash:
            return L"BOM";
        case TowerDefenseTowerType::Slow:
            return L"SLOW";
        case TowerDefenseTowerType::Mortar:
            return L"MOR";
        case TowerDefenseTowerType::Flak:
            return L"AIR";
        default:
            return L"BAS";
        }
    }

    const wchar_t* OfferShortName(const TowerDefenseOffer& offer)
    {
        switch (offer.kind)
        {
        case TowerDefenseOfferKind::Meteor:
            return L"METEO";
        case TowerDefenseOfferKind::Freeze:
            return L"SLOW";
        case TowerDefenseOfferKind::Boost:
            return L"POWER";
        case TowerDefenseOfferKind::Generator:
            return L"COIN";
        case TowerDefenseOfferKind::Boulder:
            return L"ROCK";
        default:
            return TowerTypeShortName(offer.type);
        }
    }

    const wchar_t* StagePresetName(int stage)
    {
        static constexpr const wchar_t* Names[3] = { L"MEADOW", L"RIDGE", L"SPIRAL" };
        return Names[clamp(stage, 0, 2)];
    }

    const wchar_t* DifficultyPresetName(int difficulty)
    {
        static constexpr const wchar_t* Names[3] = { L"EASY", L"NORMAL", L"HARD" };
        return Names[clamp(difficulty, 0, 2)];
    }

    float StageStartX(int stage)
    {
        static constexpr float Values[3] = { TowerDefenseRoute::StartX, 0.035f, 0.025f };
        return Values[clamp(stage, 0, 2)];
    }

    float StageMeetX(int stage)
    {
        static constexpr float Values[3] = { TowerDefenseRoute::MeetX, 0.455f, 0.565f };
        return Values[clamp(stage, 0, 2)];
    }

    float StageEndX(int stage)
    {
        static constexpr float Values[3] = { TowerDefenseRoute::EndX, 0.965f, 0.975f };
        return Values[clamp(stage, 0, 2)];
    }

    float StageBranchStartZ(int stage, int routeIndex)
    {
        static constexpr float Starts[3][TowerDefenseRoute::RouteCount] = {
            { 0.22f, 0.50f, 0.78f },
            { 0.16f, 0.48f, 0.84f },
            { 0.30f, 0.51f, 0.70f }
        };
        return Starts[clamp(stage, 0, 2)][clamp(routeIndex, 0, TowerDefenseRoute::RouteCount - 1)];
    }

    float StageSharedZ(int stage, float t)
    {
        t = clamp(t, 0.0f, 1.0f);
        switch (clamp(stage, 0, 2))
        {
        case 1:
            return TowerDefenseRoute::MeetZ + sinf(t * XM_PI * 0.82f) * 0.038f - t * 0.055f;
        case 2:
            return TowerDefenseRoute::MeetZ + sinf((t * 1.80f + 0.15f) * XM_PI) * 0.105f;
        default:
            return TowerDefenseRoute::SharedZ(t);
        }
    }

    float StageRouteCenterZ(int stage, int routeIndex, float normalizedX)
    {
        stage = clamp(stage, 0, 2);
        const float startX = StageStartX(stage);
        const float meetX = StageMeetX(stage);
        const float endX = StageEndX(stage);
        if (normalizedX <= meetX)
        {
            const float t = (normalizedX - startX) / max(0.001f, meetX - startX);
            const float startZ = StageBranchStartZ(stage, routeIndex);
            float center = startZ + (TowerDefenseRoute::MeetZ - startZ) * TowerDefenseRoute::Smooth01(t);
            if (stage == 1) center += sinf(t * XM_PI * 2.0f) * 0.030f;
            if (stage == 2) center += sinf((t + routeIndex * 0.17f) * XM_PI * 1.4f) * 0.045f;
            return center;
        }

        const float t = (normalizedX - meetX) / max(0.001f, endX - meetX);
        return StageSharedZ(stage, t);
    }

    XMFLOAT2 StagePathPointNormalized(int stage, int routeIndex, float t)
    {
        stage = clamp(stage, 0, 2);
        routeIndex = clamp(routeIndex, 0, TowerDefenseRoute::RouteCount - 1);
        t = clamp(t, 0.0f, 1.0f);

        if (stage == 2)
        {
            const float startZ = StageBranchStartZ(stage, routeIndex);
            if (t < 0.22f)
            {
                const float a = TowerDefenseRoute::Smooth01(t / 0.22f);
                return XMFLOAT2{
                    StageStartX(stage) + (0.36f - StageStartX(stage)) * a,
                    startZ + (0.50f - startZ) * a
                };
            }
            if (t < 0.78f)
            {
                const float a = (t - 0.22f) / 0.56f;
                const float loops = 1.55f;
                const float angle = XM_2PI * loops * a + static_cast<float>(routeIndex) * 0.46f;
                const float radius = 0.245f - 0.080f * a;
                return XMFLOAT2{
                    0.50f + cosf(angle) * radius,
                    0.50f + sinf(angle) * radius
                };
            }

            const float a = TowerDefenseRoute::Smooth01((t - 0.78f) / 0.22f);
            const float startAngle = XM_2PI * 1.55f + static_cast<float>(routeIndex) * 0.46f;
            const XMFLOAT2 spiralEnd{
                0.50f + cosf(startAngle) * 0.165f,
                0.50f + sinf(startAngle) * 0.165f
            };
            return XMFLOAT2{
                spiralEnd.x + (StageEndX(stage) - spiralEnd.x) * a,
                spiralEnd.y + (0.50f - spiralEnd.y) * a
            };
        }

        const float startX = StageStartX(stage);
        const float meetX = StageMeetX(stage);
        const float endX = StageEndX(stage);
        const float split = 0.50f;
        float x = 0.0f;
        if (t <= split)
        {
            const float a = t / split;
            x = startX + (meetX - startX) * a;
        }
        else
        {
            const float a = (t - split) / (1.0f - split);
            x = meetX + (endX - meetX) * a;
        }

        return XMFLOAT2{ x, clamp(StageRouteCenterZ(stage, routeIndex, x), 0.08f, 0.92f) };
    }

    float DistancePointToSegmentSq2D(const XMFLOAT2& point, const XMFLOAT2& a, const XMFLOAT2& b)
    {
        const float abx = b.x - a.x;
        const float abz = b.y - a.y;
        const float lengthSq = abx * abx + abz * abz;
        if (lengthSq <= 0.000001f)
        {
            const float dx = point.x - a.x;
            const float dz = point.y - a.y;
            return dx * dx + dz * dz;
        }

        float u = ((point.x - a.x) * abx + (point.y - a.y) * abz) / lengthSq;
        u = clamp(u, 0.0f, 1.0f);
        const float cx = a.x + abx * u;
        const float cz = a.y + abz * u;
        const float dx = point.x - cx;
        const float dz = point.y - cz;
        return dx * dx + dz * dz;
    }

    float StageRoadMask(int stage, float nx, float nz, float innerHalfWidth, float outerHalfWidth)
    {
        if (stage == 2)
        {
            const XMFLOAT2 point{ nx, nz };
            float bestDistanceSq = outerHalfWidth * outerHalfWidth * 4.0f;
            for (int route = 0; route < TowerDefenseRoute::RouteCount; ++route)
            {
                XMFLOAT2 previous = StagePathPointNormalized(stage, route, 0.0f);
                for (int i = 1; i <= 72; ++i)
                {
                    const float t = static_cast<float>(i) / 72.0f;
                    const XMFLOAT2 current = StagePathPointNormalized(stage, route, t);
                    bestDistanceSq = min(bestDistanceSq, DistancePointToSegmentSq2D(point, previous, current));
                    previous = current;
                }
            }

            const float distance = sqrtf(max(0.0f, bestDistanceSq));
            const float edge = 1.0f - TowerDefenseRoute::Smooth01((distance - innerHalfWidth) / max(0.001f, outerHalfWidth - innerHalfWidth));
            return clamp(edge, 0.0f, 1.0f);
        }

        if (nx < StageStartX(stage) - 0.006f || nx > StageEndX(stage) + 0.015f) return 0.0f;

        float mask = 0.0f;
        for (int route = 0; route < TowerDefenseRoute::RouteCount; ++route)
        {
            const float offset = fabsf(nz - StageRouteCenterZ(stage, route, nx));
            const float edge = 1.0f - TowerDefenseRoute::Smooth01((offset - innerHalfWidth) / max(0.001f, outerHalfWidth - innerHalfWidth));
            mask = max(mask, clamp(edge, 0.0f, 1.0f));
        }
        return mask;
    }

    shared_ptr<TerrainHeightMap> CreateStageHeightMap(int stage, UINT width, UINT length, float cellSpacing)
    {
        stage = clamp(stage, 0, 2);
        width = max(width, 2u);
        length = max(length, 2u);

        vector<float> heights(static_cast<size_t>(width) * length);
        vector<float> roadMask(heights.size());
        const float amplitude[3]{ 2.15f, 2.85f, 2.45f };
        const float ridgeStrength[3]{ 0.72f, 1.42f, 0.92f };
        const float worldWidth = static_cast<float>(width - 1) * cellSpacing;

        for (UINT z = 0; z < length; ++z)
        {
            for (UINT x = 0; x < width; ++x)
            {
                const float nx = static_cast<float>(x) / static_cast<float>(width - 1);
                const float nz = static_cast<float>(z) / static_cast<float>(length - 1);
                const float waveA = sinf((nx * (2.25f + stage * 0.35f) + nz * 0.45f) * XM_2PI);
                const float waveB = cosf((nz * (2.60f + stage * 0.20f) - nx * 0.28f) * XM_2PI);
                const float ridgeLine = 1.0f - fabsf(sinf((nx * 1.32f + nz * (0.78f + stage * 0.17f)) * XM_2PI));
                const float ridge = powf(max(0.0f, ridgeLine), 3.0f) * ridgeStrength[stage];
                const float hillA = expf(-17.0f * ((nx - 0.22f) * (nx - 0.22f) + (nz - 0.25f) * (nz - 0.25f)));
                const float hillB = expf(-13.0f * ((nx - 0.76f) * (nx - 0.76f) + (nz - 0.76f) * (nz - 0.76f)));
                const float basin = -0.62f * expf(-14.0f * ((nx - 0.54f) * (nx - 0.54f) + (nz - 0.54f) * (nz - 0.54f)));
                const float road = StageRoadMask(stage, nx, nz, 0.018f, 0.070f);
                const float roadCore = StageRoadMask(stage, nx, nz, 0.010f, 0.046f);
                const float naturalHeight = waveA * 0.32f + waveB * 0.24f + ridge + hillA * 0.88f + hillB * 0.75f + basin;
                const float roadHeight = waveA * 0.050f + waveB * 0.038f - 0.42f * road + basin * 0.22f;
                const float blend = roadCore * 0.86f;
                const size_t index = static_cast<size_t>(z) * width + x;
                float height = (naturalHeight + (roadHeight - naturalHeight) * blend) * amplitude[stage];
                if (stage == 1)
                {
                    const float downhill = (1.0f - nx) * worldWidth * 0.50f;
                    const float ridgeShelf = expf(-18.0f * ((nx - 0.18f) * (nx - 0.18f))) * 3.8f;
                    height += downhill + ridgeShelf;
                    height -= road * 1.35f;
                }
                else if (stage == 2)
                {
                    const float dx = nx - 0.50f;
                    const float dz = nz - 0.50f;
                    const float radial = sqrtf(dx * dx + dz * dz);
                    const float outerWall = powf(clamp((radial - 0.22f) / 0.34f, 0.0f, 1.0f), 1.8f) * 5.2f;
                    const float innerBasin = -2.5f * expf(-18.0f * radial * radial);
                    height += outerWall + innerBasin - road * 0.85f;
                }

                heights[index] = height;
                roadMask[index] = road;
            }
        }

        return make_shared<TerrainHeightMap>(width, length, cellSpacing, std::move(heights), std::move(roadMask));
    }

    bool IsConsumableOffer(TowerDefenseOfferKind kind)
    {
        return kind == TowerDefenseOfferKind::Meteor ||
            kind == TowerDefenseOfferKind::Freeze ||
            kind == TowerDefenseOfferKind::Boost ||
            kind == TowerDefenseOfferKind::Boulder;
    }

    float MeteorRadius(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 2.75f + static_cast<float>(tier) * 0.70f;
    }

    float MeteorDamage(int tier, int wave)
    {
        tier = clamp(tier, 1, 3);
        return 72.0f + static_cast<float>(tier) * 42.0f + static_cast<float>(max(1, wave) - 1) * 11.0f;
    }

    float FreezeRadius(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 3.20f + static_cast<float>(tier) * 0.65f;
    }

    float FreezeDuration(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 2.65f + static_cast<float>(tier) * 0.55f;
    }

    float BoostDuration(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 7.0f + static_cast<float>(tier) * 2.0f;
    }

    float BoostMultiplier(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 1.25f + static_cast<float>(tier) * 0.18f;
    }

    float BoulderRadius(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 0.62f + static_cast<float>(tier) * 0.18f;
    }

    float BoulderDamage(int tier, int wave)
    {
        tier = clamp(tier, 1, 3);
        return 64.0f + static_cast<float>(tier) * 42.0f + static_cast<float>(max(1, wave) - 1) * 13.0f;
    }

    float BoulderSpeed(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 4.4f + static_cast<float>(tier) * 0.78f;
    }

    float GeneratorInterval(int tier)
    {
        tier = clamp(tier, 1, 3);
        return max(3.6f, 7.3f - static_cast<float>(tier) * 0.9f);
    }

    int GeneratorAmount(int tier)
    {
        tier = clamp(tier, 1, 3);
        return tier;
    }

    XMFLOAT3 MoveTowards(const XMFLOAT3& current, const XMFLOAT3& target, float maxDistance)
    {
        XMFLOAT3 delta = Utiles::Vector3::Sub(target, current);
        float lengthSq = Utiles::Vector3::Dot(delta, delta);
        if (lengthSq <= maxDistance * maxDistance || lengthSq <= Utiles::Physics::Epsilon) return target;

        float invLength = 1.0f / sqrtf(lengthSq);
        return XMFLOAT3{
            current.x + delta.x * invLength * maxDistance,
            current.y + delta.y * invLength * maxDistance,
            current.z + delta.z * invLength * maxDistance
        };
    }

    XMFLOAT3 Normalize(const XMFLOAT3& value)
    {
        float lengthSq = Utiles::Vector3::Dot(value, value);
        if (lengthSq <= Utiles::Physics::Epsilon) return XMFLOAT3{ 0.0f, 0.0f, 1.0f };

        float invLength = 1.0f / sqrtf(lengthSq);
        return XMFLOAT3{ value.x * invLength, value.y * invLength, value.z * invLength };
    }

    XMFLOAT4 TowerColor(TowerDefenseTowerType type, int tier, float alpha = 1.0f)
    {
        tier = clamp(tier, 1, 3);
        const float tierBoost = static_cast<float>(tier - 1) * 0.18f;
        XMFLOAT3 color{};
        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            color = XMFLOAT3{ 1.0f, 0.66f, 0.08f };
            break;
        case TowerDefenseTowerType::Splash:
            color = XMFLOAT3{ 1.0f, 0.18f, 0.12f };
            break;
        case TowerDefenseTowerType::Slow:
            color = XMFLOAT3{ 0.12f, 0.90f, 1.0f };
            break;
        case TowerDefenseTowerType::Mortar:
            color = XMFLOAT3{ 0.46f, 0.92f, 0.20f };
            break;
        case TowerDefenseTowerType::Flak:
            color = XMFLOAT3{ 0.86f, 0.42f, 1.0f };
            break;
        default:
            color = XMFLOAT3{ 0.26f, 0.64f, 1.0f };
            break;
        }

        return XMFLOAT4{
            clamp(color.x + tierBoost, 0.0f, 1.0f),
            clamp(color.y + tierBoost, 0.0f, 1.0f),
            clamp(color.z + tierBoost, 0.0f, 1.0f),
            alpha
        };
    }

    float TowerVisualScale(int tier)
    {
        tier = clamp(tier, 1, 3);
        return 1.12f + static_cast<float>(tier - 1) * 0.26f;
    }

    XMFLOAT3 TowerVisualSize(TowerDefenseTowerType type, int tier)
    {
        const float tierScale = TowerVisualScale(tier);
        switch (type)
        {
        case TowerDefenseTowerType::Mortar:
            return XMFLOAT3{ 0.50f * tierScale, 0.42f * tierScale, 0.50f * tierScale };
        case TowerDefenseTowerType::Flak:
            return XMFLOAT3{ 0.26f * tierScale, 0.82f * tierScale, 0.26f * tierScale };
        case TowerDefenseTowerType::Splash:
            return XMFLOAT3{ 0.42f * tierScale, 0.56f * tierScale, 0.42f * tierScale };
        case TowerDefenseTowerType::Rapid:
            return XMFLOAT3{ 0.24f * tierScale, 0.68f * tierScale, 0.24f * tierScale };
        default:
            return XMFLOAT3{ 0.32f * tierScale, 0.60f * tierScale, 0.32f * tierScale };
        }
    }

    XMFLOAT4 ProjectileColor(TowerDefenseTowerType type)
    {
        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            return XMFLOAT4{ 1.0f, 0.72f, 0.10f, 1.0f };
        case TowerDefenseTowerType::Splash:
            return XMFLOAT4{ 1.0f, 0.22f, 0.08f, 1.0f };
        case TowerDefenseTowerType::Slow:
            return XMFLOAT4{ 0.18f, 0.86f, 1.0f, 1.0f };
        case TowerDefenseTowerType::Mortar:
            return XMFLOAT4{ 0.62f, 1.0f, 0.16f, 1.0f };
        case TowerDefenseTowerType::Flak:
            return XMFLOAT4{ 0.95f, 0.46f, 1.0f, 1.0f };
        default:
            return XMFLOAT4{ 0.62f, 0.82f, 1.0f, 1.0f };
        }
    }

    XMFLOAT4 HitColor(TowerDefenseTowerType type)
    {
        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            return XMFLOAT4{ 1.0f, 0.86f, 0.22f, 1.0f };
        case TowerDefenseTowerType::Splash:
            return XMFLOAT4{ 1.0f, 0.38f, 0.12f, 1.0f };
        case TowerDefenseTowerType::Slow:
            return XMFLOAT4{ 0.26f, 0.94f, 1.0f, 1.0f };
        case TowerDefenseTowerType::Mortar:
            return XMFLOAT4{ 0.78f, 1.0f, 0.22f, 1.0f };
        case TowerDefenseTowerType::Flak:
            return XMFLOAT4{ 1.0f, 0.48f, 1.0f, 1.0f };
        default:
            return XMFLOAT4{ 0.90f, 0.96f, 1.0f, 1.0f };
        }
    }

    struct TowerStats
    {
        float minRange = 0.0f;
        float range = 3.0f;
        float damage = 20.0f;
        float fireInterval = 0.55f;
        float splashRadius = 0.0f;
        float slowDuration = 0.0f;
        float slowMultiplier = 1.0f;
        bool targetsGround = true;
        bool targetsAir = false;
    };

    TowerStats BuildTowerStats(TowerDefenseTowerType type, int tier)
    {
        tier = clamp(tier, 1, 3);
        const float t = static_cast<float>(tier);

        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            return TowerStats{ 0.0f, 3.20f + t * 0.24f, 6.5f + t * 4.0f, max(0.19f, 0.32f - t * 0.025f) };
        case TowerDefenseTowerType::Splash:
            return TowerStats{ 0.0f, 3.50f + t * 0.36f, 11.0f + t * 7.5f, max(0.55f, 0.92f - t * 0.085f), 0.82f + t * 0.22f };
        case TowerDefenseTowerType::Slow:
            return TowerStats{ 0.0f, 3.85f + t * 0.34f, 7.0f + t * 4.5f, max(0.42f, 0.76f - t * 0.045f), 0.0f, 1.25f + t * 0.22f, max(0.38f, 0.68f - t * 0.07f) };
        case TowerDefenseTowerType::Mortar:
            return TowerStats{ 3.45f + t * 0.18f, 8.20f + t * 0.92f, 26.0f + t * 18.0f, max(0.90f, 1.62f - t * 0.11f), 1.25f + t * 0.38f, 0.0f, 1.0f, true, false };
        case TowerDefenseTowerType::Flak:
            return TowerStats{ 0.0f, 7.10f + t * 0.85f, 12.0f + t * 8.5f, max(0.28f, 0.55f - t * 0.055f), 0.72f + t * 0.22f, 0.0f, 1.0f, false, true };
        default:
            return TowerStats{ 0.0f, 3.30f + t * 0.46f, 16.0f + t * 8.0f, max(0.40f, 0.62f - t * 0.06f) };
        }
    }

    float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        const float dx = a.x - b.x;
        const float dz = a.z - b.z;
        return dx * dx + dz * dz;
    }

    XMFLOAT3 LerpPoint(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return XMFLOAT3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    bool GetClientCursor(HWND hWnd, POINT& cursor, float& width, float& height)
    {
        if (!GetCursorPos(&cursor)) return false;
        ScreenToClient(hWnd, &cursor);

        RECT clientRect{};
        if (!GetClientRect(hWnd, &clientRect)) return false;

        width = static_cast<float>(clientRect.right - clientRect.left);
        height = static_cast<float>(clientRect.bottom - clientRect.top);
        if (width <= 1.0f || height <= 1.0f) return false;

        return cursor.x >= 0 && cursor.y >= 0 &&
            static_cast<float>(cursor.x) < width &&
            static_cast<float>(cursor.y) < height;
    }

    bool BuildMouseRay(HWND hWnd, const shared_ptr<Camera>& camera,
        float fovY, XMFLOAT3& outOrigin, XMFLOAT3& outDirection,
        POINT* outCursor = nullptr, XMFLOAT2* outClientSize = nullptr)
    {
        if (!camera) return false;

        POINT cursor{};
        float width = 0.0f;
        float height = 0.0f;
        if (!GetClientCursor(hWnd, cursor, width, height)) return false;

        const float ndcX = (static_cast<float>(cursor.x) + 0.5f) / width * 2.0f - 1.0f;
        const float ndcY = 1.0f - (static_cast<float>(cursor.y) + 0.5f) / height * 2.0f;
        const float tanHalfFov = tanf(fovY * 0.5f);
        const float aspect = width / height;

        outOrigin = camera->GetEye();
        outDirection = Utiles::Vector3::Add(
            camera->GetN(),
            Utiles::Vector3::Add(
                Utiles::Vector3::Mul(camera->GetU(), ndcX * tanHalfFov * aspect),
                Utiles::Vector3::Mul(camera->GetV(), ndcY * tanHalfFov)));
        outDirection = Normalize(outDirection);

        if (outCursor) *outCursor = cursor;
        if (outClientSize) *outClientSize = XMFLOAT2{ width, height };
        return true;
    }

    filesystem::path ModuleDirectory()
    {
        WCHAR modulePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
        if (length == 0 || length == _countof(modulePath)) return {};
        return filesystem::path(modulePath).parent_path();
    }

    filesystem::path ResolveProjectAssetPath(const filesystem::path& relativePath)
    {
        vector<filesystem::path> roots;
        roots.push_back(filesystem::current_path());

        filesystem::path moduleDir = ModuleDirectory();
        if (!moduleDir.empty()) roots.push_back(moduleDir);

        for (size_t i = 0; i < roots.size(); ++i)
        {
            filesystem::path root = roots[i];
            for (int depth = 0; depth < 7 && !root.empty(); ++depth)
            {
                vector<filesystem::path> candidates{
                    root / relativePath,
                    root / "GameJam" / "GameJam" / relativePath,
                    root / "GameJam" / relativePath
                };

                for (const auto& candidate : candidates)
                {
                    if (filesystem::exists(candidate)) return filesystem::absolute(candidate);
                }

                if (!root.has_parent_path()) break;
                filesystem::path parent = root.parent_path();
                if (parent == root) break;
                root = parent;
            }
        }

        return {};
    }

    struct TowerPartManifestRecord
    {
        string name;
        string meshFile;
        string materialFile;
        XMFLOAT4X4 localMatrix{};
        bool rotatesWithTarget = false;
    };

    XMFLOAT4X4 IdentityMatrix()
    {
        XMFLOAT4X4 matrix{};
        XMStoreFloat4x4(&matrix, XMMatrixIdentity());
        return matrix;
    }

    string LowerCopy(string value)
    {
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
        return value;
    }

    bool IsTurretPartName(const string& name)
    {
        const string lower = LowerCopy(name);
        return lower.find("turret") != string::npos ||
            lower.find("barrel") != string::npos ||
            lower.find("gun") != string::npos ||
            lower.find("cannon") != string::npos ||
            lower.find("head") != string::npos ||
            lower.find("top") != string::npos;
    }

    float NormalizeAngle(float angle)
    {
        while (angle > XM_PI) angle -= XM_2PI;
        while (angle < -XM_PI) angle += XM_2PI;
        return angle;
    }

    float MoveAngleTowards(float current, float target, float maxStep)
    {
        const float delta = NormalizeAngle(target - current);
        if (fabsf(delta) <= maxStep) return target;
        return NormalizeAngle(current + (delta < 0.0f ? -maxStep : maxStep));
    }

    bool ReadTowerModelManifest(const filesystem::path& manifestPath,
        vector<TowerPartManifestRecord>& outParts)
    {
        ifstream input(manifestPath);
        if (!input) return false;

        string token;
        int version = 0;
        input >> token >> version;
        if (token != "GameJamTowerModel" || version <= 0) return false;

        TowerPartManifestRecord current{};
        current.localMatrix = IdentityMatrix();
        bool readingPart = false;

        while (input >> token)
        {
            if (token == "Root" || token == "CombinedMesh" || token == "CombinedMaterial" || token == "Parts")
            {
                string ignored;
                input >> ignored;
            }
            else if (token == "Part")
            {
                current = TowerPartManifestRecord{};
                current.localMatrix = IdentityMatrix();
                input >> current.name;
                readingPart = true;
            }
            else if (token == "Mesh" && readingPart)
            {
                input >> current.meshFile;
            }
            else if (token == "Material" && readingPart)
            {
                input >> current.materialFile;
            }
            else if (token == "Rotates" && readingPart)
            {
                int enabled = 0;
                input >> enabled;
                current.rotatesWithTarget = enabled != 0;
            }
            else if (token == "LocalMatrix" && readingPart)
            {
                input >> current.localMatrix._11 >> current.localMatrix._12 >> current.localMatrix._13 >> current.localMatrix._14;
                input >> current.localMatrix._21 >> current.localMatrix._22 >> current.localMatrix._23 >> current.localMatrix._24;
                input >> current.localMatrix._31 >> current.localMatrix._32 >> current.localMatrix._33 >> current.localMatrix._34;
                input >> current.localMatrix._41 >> current.localMatrix._42 >> current.localMatrix._43 >> current.localMatrix._44;
            }
            else if (token == "EndPart" && readingPart)
            {
                if (!current.meshFile.empty() && !current.materialFile.empty())
                {
                    current.rotatesWithTarget = current.rotatesWithTarget || IsTurretPartName(current.name);
                    outParts.push_back(current);
                }
                readingPart = false;
            }
            else if (token == "EndModel")
            {
                break;
            }
        }

        return !outParts.empty();
    }

}

void TowerDefenseScene::BuildObjects(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const ComPtr<ID3D12RootSignature>& rootSignature)
{
    m_device = device;
    m_shader = make_shared<Shader>(device, rootSignature,
        "PIXEL_SHADOW_MAIN",
        true,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        "VERTEX_SHADOW_MAIN");
    m_overlayShader = make_shared<Shader>(device, rootSignature, "PIXEL_UNLIT", false);
    m_debugLineShader = make_shared<Shader>(device, rootSignature,
        "PIXEL_VERTEX_COLOR_UNLIT", false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    m_shadowMap = make_unique<ShadowMap>();
    m_shadowMap->Initialize(device, rootSignature, 2048);
    m_cube = make_shared<CubeMesh>(device, commandList);
    m_capsuleMesh = make_shared<CapsuleIndexMesh>(device, commandList,
        EnemyCapsuleRadius, EnemyCapsuleBodyHeight, 20);
    m_boulderMesh = make_shared<CapsuleIndexMesh>(device, commandList, 1.0f, 0.0f, 28);
    BuildStageTerrainAssets(device, commandList);

    m_camera = make_shared<SpectatorCamera>();
    ConfigureCameraLens(m_camera);
    UpdateGameplayCamera(0.0f);

    m_sunLight = make_shared<PointLight>();
    m_sunLight->SetRange(220.0f);
    m_sunLight->SetIntensity(190.0f);
    m_sunLight->SetColor(XMFLOAT3{ 1.0f, 0.94f, 0.66f });
    m_lights.push_back(m_sunLight);
    m_moonLight = make_shared<PointLight>();
    m_moonLight->SetRange(180.0f);
    m_moonLight->SetIntensity(0.0f);
    m_moonLight->SetColor(XMFLOAT3{ 0.58f, 0.68f, 1.0f });
    m_lights.push_back(m_moonLight);
    UpdateCelestialCycle(0.0f);

    LoadBitmapFont();
    BuildMaterials(device);
    BuildTowerModelAssets(device, commandList);
    BuildBitmapTextPool();
    BuildStartScreen();
}

void TowerDefenseScene::BuildMaterials(const ComPtr<ID3D12Device>& device)
{
    m_startMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.15f, 1.0f, 0.36f, 1.0f });
    m_startMaterial->SetEmission(XMFLOAT3{ 0.08f, 0.8f, 0.2f }, 0.45f);

    m_startAccentMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.9f, 1.0f, 0.45f, 1.0f });
    m_startAccentMaterial->SetEmission(XMFLOAT3{ 0.6f, 0.6f, 0.12f }, 0.25f);

    m_fieldMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
    m_fieldMaterial->SetTerrainTexture(0.18f);
    m_blockedMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.24f, 0.29f, 0.36f, 1.0f });
    m_shopPanelMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.025f, 0.032f, 0.040f, 0.94f });
    m_shopPanelMaterial->SetEmission(XMFLOAT3{ 0.01f, 0.05f, 0.08f }, 0.22f);
    m_shopSlotMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.13f, 0.16f, 0.19f, 0.94f });
    m_shopSlotMaterial->SetEmission(XMFLOAT3{ 0.04f, 0.10f, 0.14f }, 0.42f);
    m_infoBarBackgroundMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.010f, 0.014f, 0.018f, 0.96f });
    m_infoDamageMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.34f, 0.22f, 0.95f });
    m_infoDamageMaterial->SetEmission(XMFLOAT3{ 0.55f, 0.08f, 0.04f }, 0.25f);
    m_infoRangeMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.18f, 0.92f, 1.0f, 0.95f });
    m_infoRangeMaterial->SetEmission(XMFLOAT3{ 0.06f, 0.48f, 0.58f }, 0.28f);
    m_infoFireRateMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.78f, 0.22f, 0.95f });
    m_infoFireRateMaterial->SetEmission(XMFLOAT3{ 0.58f, 0.35f, 0.04f }, 0.25f);
    m_infoCooldownMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.48f, 1.0f, 0.40f, 0.95f });
    m_infoCooldownMaterial->SetEmission(XMFLOAT3{ 0.12f, 0.46f, 0.08f }, 0.25f);

    for (int type = 0; type < TowerTypeCount; ++type)
    {
        const TowerDefenseTowerType towerType = TowerTypeFromIndex(type);
        for (int tier = 1; tier <= MaxTowerTier; ++tier)
        {
            const XMFLOAT4 color = TowerColor(towerType, tier);
            m_towerMaterials[type][tier - 1] = Material::Create(device, m_shader, color);
            m_shopTowerMaterials[type][tier - 1] = Material::Create(device, m_overlayShader, color);
            m_shopTowerMaterials[type][tier - 1]->SetEmission(XMFLOAT3{ color.x * 0.26f, color.y * 0.26f, color.z * 0.26f }, 0.38f);
        }
    }
    m_healthBarBackMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.05f, 0.06f, 0.07f, 0.86f });
    m_healthBarFillMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.25f, 1.0f, 0.32f, 0.92f });
    m_healthBarFillMaterial->SetEmission(XMFLOAT3{ 0.05f, 0.45f, 0.09f }, 0.30f);
    m_deathEffectMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.30f, 0.16f, 1.0f });
    m_deathEffectMaterial->SetEmission(XMFLOAT3{ 0.90f, 0.12f, 0.04f }, 1.25f);
    m_coinMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.86f, 0.16f, 1.0f });
    m_coinMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.58f, 0.04f }, 1.65f);
    m_goldUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.82f, 0.16f, 0.95f });
    m_goldUiMaterial->SetEmission(XMFLOAT3{ 0.90f, 0.48f, 0.04f }, 0.70f);
    m_goldDigitMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.92f, 0.22f, 1.0f });
    m_goldDigitMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.65f, 0.08f }, 0.88f);
    m_bitmapTextMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.98f, 1.0f, 0.96f, 1.0f });
    m_bitmapTextMaterial->SetEmission(XMFLOAT3{ 0.82f, 0.95f, 0.78f }, 0.78f);
    m_resultVictoryMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.18f, 0.95f, 0.38f, 0.88f });
    m_resultVictoryMaterial->SetEmission(XMFLOAT3{ 0.08f, 0.70f, 0.20f }, 0.70f);
    m_resultDefeatMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.22f, 0.18f, 0.88f });
    m_resultDefeatMaterial->SetEmission(XMFLOAT3{ 0.80f, 0.05f, 0.04f }, 0.70f);
    m_bossMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.66f, 0.08f, 0.95f, 1.0f });
    m_bossMaterial->SetEmission(XMFLOAT3{ 0.46f, 0.02f, 0.78f }, 0.55f);
    m_flyingEnemyMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.42f, 0.72f, 1.0f, 1.0f });
    m_flyingEnemyMaterial->SetEmission(XMFLOAT3{ 0.12f, 0.38f, 1.0f }, 0.75f);
    m_runnerEnemyMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.58f, 0.14f, 1.0f });
    m_runnerEnemyMaterial->SetEmission(XMFLOAT3{ 0.78f, 0.30f, 0.04f }, 0.55f);
    m_armoredEnemyMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.52f, 0.58f, 0.66f, 1.0f });
    m_armoredEnemyMaterial->SetEmission(XMFLOAT3{ 0.14f, 0.16f, 0.22f }, 0.22f);
    m_splitterEnemyMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.36f, 1.0f, 0.74f, 1.0f });
    m_splitterEnemyMaterial->SetEmission(XMFLOAT3{ 0.05f, 0.72f, 0.42f }, 0.78f);
    m_bossHealthFillMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.16f, 0.34f, 0.96f });
    m_bossHealthFillMaterial->SetEmission(XMFLOAT3{ 0.78f, 0.03f, 0.12f }, 0.85f);
    m_lifeUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.20f, 0.30f, 0.95f });
    m_lifeUiMaterial->SetEmission(XMFLOAT3{ 0.75f, 0.05f, 0.10f }, 0.70f);
    m_mergeHighlightMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.94f, 0.25f, 0.78f });
    m_mergeHighlightMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.75f, 0.08f }, 1.60f);
    m_meteorMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.42f, 0.12f, 1.0f });
    m_meteorMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.24f, 0.05f }, 2.40f);
    m_meteorUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.48f, 0.14f, 0.96f });
    m_meteorUiMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.30f, 0.06f }, 0.95f);
    m_freezeMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.28f, 0.88f, 1.0f, 0.86f });
    m_freezeMaterial->SetEmission(XMFLOAT3{ 0.08f, 0.58f, 1.0f }, 1.45f);
    m_freezeUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.24f, 0.82f, 1.0f, 0.96f });
    m_freezeUiMaterial->SetEmission(XMFLOAT3{ 0.06f, 0.52f, 1.0f }, 0.85f);
    m_boostMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.34f, 1.0f, 0.24f, 0.92f });
    m_boostMaterial->SetEmission(XMFLOAT3{ 0.12f, 0.86f, 0.12f }, 1.35f);
    m_boostUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.36f, 1.0f, 0.28f, 0.96f });
    m_boostUiMaterial->SetEmission(XMFLOAT3{ 0.12f, 0.76f, 0.08f }, 0.82f);
    m_boulderMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.50f, 0.47f, 0.40f, 1.0f });
    m_boulderMaterial->SetEmission(XMFLOAT3{ 0.08f, 0.07f, 0.05f }, 0.22f);
    m_boulderUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.56f, 0.52f, 0.42f, 0.96f });
    m_boulderUiMaterial->SetEmission(XMFLOAT3{ 0.18f, 0.14f, 0.08f }, 0.45f);
    m_generatorMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.76f, 0.18f, 1.0f });
    m_generatorMaterial->SetEmission(XMFLOAT3{ 0.92f, 0.46f, 0.05f }, 0.85f);
    m_generatorUiMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.78f, 0.20f, 0.96f });
    m_generatorUiMaterial->SetEmission(XMFLOAT3{ 0.90f, 0.48f, 0.06f }, 0.82f);
    m_shopConsumableSlotMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.30f, 0.14f, 0.08f, 0.90f });
    m_shopConsumableSlotMaterial->SetEmission(XMFLOAT3{ 0.30f, 0.08f, 0.02f }, 0.40f);
    m_shopGeneratorSlotMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.28f, 0.22f, 0.08f, 0.90f });
    m_shopGeneratorSlotMaterial->SetEmission(XMFLOAT3{ 0.34f, 0.20f, 0.04f }, 0.42f);

    m_dragGhostMaterial = Material::Create(device, m_shader, TowerColor(TowerDefenseTowerType::Basic, 1, 0.38f));
    m_dragGhostMaterial->SetEmission(XMFLOAT3{ 0.30f, 0.55f, 1.0f }, 0.35f);

    m_enemyMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.20f, 0.18f, 1.0f });
    m_hitMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.95f, 0.25f, 1.0f });
    m_hitMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.85f, 0.15f }, 0.75f);
    m_scopeMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.18f, 0.92f, 1.0f, 0.74f });
    m_scopeMaterial->SetEmission(XMFLOAT3{ 0.12f, 0.88f, 1.0f }, 1.45f);
    m_projectileMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.86f, 0.18f, 1.0f });
    m_projectileMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.62f, 0.08f }, 2.75f);
    for (int type = 0; type < TowerTypeCount; ++type)
    {
        const TowerDefenseTowerType towerType = TowerTypeFromIndex(type);
        const XMFLOAT4 projectileColor = ProjectileColor(towerType);
        m_projectileMaterials[type] = Material::Create(device, m_shader, projectileColor);
        m_projectileMaterials[type]->SetEmission(
            XMFLOAT3{ projectileColor.x, projectileColor.y, projectileColor.z },
            towerType == TowerDefenseTowerType::Mortar ? 3.80f :
            towerType == TowerDefenseTowerType::Splash ? 3.15f : 2.65f);

        const XMFLOAT4 hitColor = HitColor(towerType);
        m_hitMaterials[type] = Material::Create(device, m_shader, hitColor);
        m_hitMaterials[type]->SetEmission(
            XMFLOAT3{ hitColor.x, hitColor.y, hitColor.z },
            towerType == TowerDefenseTowerType::Mortar ? 1.75f :
            towerType == TowerDefenseTowerType::Slow ? 1.35f : 1.05f);
    }
    m_tunnelOpeningMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.012f, 0.010f, 0.009f, 1.0f });
    m_tunnelStoneMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.23f, 0.23f, 0.21f, 1.0f });
    m_sunMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.95f, 0.34f, 1.0f });
    m_sunMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.90f, 0.24f }, 30.0f);
    m_moonMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.70f, 0.78f, 1.0f, 1.0f });
    m_moonMaterial->SetEmission(XMFLOAT3{ 0.44f, 0.52f, 1.0f }, 5.5f);
}

void TowerDefenseScene::BuildPath()
{
    if (!m_terrainHeightMap) return;

    m_waypoints.clear();
    m_enemyPaths.clear();

    const float width = m_terrainHeightMap->GetWorldWidth();
    const float length = m_terrainHeightMap->GetWorldLength();
    const int stage = clamp(m_selectedStage, 0, StagePresetCount - 1);
    const int waypointCount = stage == 2 ? 58 : 34;

    for (int route = 0; route < TowerDefenseRoute::RouteCount; ++route)
    {
        vector<XMFLOAT3> path;
        path.reserve(waypointCount);

        for (int i = 0; i < waypointCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(waypointCount - 1);
            const XMFLOAT2 point = StagePathPointNormalized(stage, route, t);
            path.push_back(TerrainWorldPosition(width * point.x, length * point.y, EnemyHalfHeight));
        }

        if (route == 1) m_waypoints = path;
        m_enemyPaths.push_back(std::move(path));
    }
}

void TowerDefenseScene::BuildStartScreen()
{
    m_mode = TowerDefenseMode::StartScreen;
    m_objects.clear();
    m_towers.clear();
    m_generators.clear();
    m_enemies.clear();
    m_hitMarkers.clear();
    m_damagePopups.clear();
    m_rollingBoulders.clear();
    m_scopeMarkers.clear();
    m_projectiles.clear();
    m_waypoints.clear();
    m_enemyPaths.clear();
    ClearDragGhost();
    m_selectedTower.reset();
    m_shopPanel.reset();
    m_shopSlots.clear();
    m_towerInfoWidgets.clear();
    m_goldUiWidgets.clear();
    m_waveUiWidgets.clear();
    m_bossHealthWidgets.clear();
    m_hudWidgets.clear();
    m_selectedTowerRangeMarkers.clear();
    m_mergeCandidateMarkers.clear();
    m_bitmapTextCache.clear();
    m_sunObject.reset();
    m_moonObject.reset();
    m_rightMouseOrbiting = false;
    m_shopCollapsed = false;
    m_topDownView = false;
    m_gameSpeedIndex = 0;
    m_cameraYaw = 0.0f;
    m_cameraPitch = DefaultOrbitPitch;
    m_cameraShakeTimer = 0.0f;
    m_cameraShakeDuration = 0.0f;
    m_cameraShakeIntensity = 0.0f;
    m_bossIntroTimer = 0.0f;
    m_consumableCooldown = 0.0f;
    m_waveRunning = false;
    m_bossRewardPending = false;
    m_bossRewardWave = 0;
    m_waveRewardPending = false;
    m_waveRewardWave = 0;
    for (int& level : m_towerDamageLevels) level = 0;
    m_totalEnemiesDefeated = 0;
    m_bossesDefeated = 0;
    m_towersPlaced = 0;
    m_towersMerged = 0;
    m_highestTowerTier = 1;
    m_goldEarned = 0;
    m_wavesCleared = 0;
    m_cameraFocus = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    m_cameraZoom = 42.0f;
    UpdateGameplayCamera(0.0f);
    if (m_collisionManager) m_collisionManager->ClearColliders();
    m_physicsManager = make_unique<PhysicsManager>();
    m_terrainCollider.reset();
    m_terrainObject.reset();

    BuildSun();
    BuildMoon();
    m_objects.push_back(CreateCubeObject("Start Screen Base",
        XMFLOAT3{ 0.0f, -0.08f, 0.0f },
        XMFLOAT3{ 5.2f, 0.08f, 3.1f },
        m_blockedMaterial));

    m_objects.push_back(CreateCubeObject("Start Button",
        XMFLOAT3{ 0.0f, 0.25f, 0.0f },
        XMFLOAT3{ 1.85f, 0.22f, 0.72f },
        m_startMaterial));

    m_objects.push_back(CreateCubeObject("Start Button Arrow A",
        XMFLOAT3{ -0.25f, 0.62f, -0.18f },
        XMFLOAT3{ 0.23f, 0.12f, 0.23f },
        m_startAccentMaterial));
    m_objects.push_back(CreateCubeObject("Start Button Arrow B",
        XMFLOAT3{ 0.15f, 0.62f, 0.0f },
        XMFLOAT3{ 0.23f, 0.12f, 0.23f },
        m_startAccentMaterial));
    m_objects.push_back(CreateCubeObject("Start Button Arrow C",
        XMFLOAT3{ -0.25f, 0.62f, 0.18f },
        XMFLOAT3{ 0.23f, 0.12f, 0.23f },
        m_startAccentMaterial));

    m_hudWidgets.clear();
    m_hudWidgets.reserve(48);
    for (int i = 0; i < 48; ++i)
    {
        m_hudWidgets.push_back(CreateCubeObject("Start Menu Widget",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_shopPanelMaterial));
    }
}

void TowerDefenseScene::StartGame()
{
    m_mode = TowerDefenseMode::Playing;
    m_objects.clear();
    m_towers.clear();
    m_generators.clear();
    m_enemies.clear();
    m_hitMarkers.clear();
    m_damagePopups.clear();
    m_rollingBoulders.clear();
    m_scopeMarkers.clear();
    m_projectiles.clear();
    ClearDragGhost();
    m_selectedTower.reset();
    m_enemyPaths.clear();
    m_shopPanel.reset();
    m_shopSlots.clear();
    m_towerInfoWidgets.clear();
    m_goldUiWidgets.clear();
    m_waveUiWidgets.clear();
    m_bossHealthWidgets.clear();
    m_hudWidgets.clear();
    m_selectedTowerRangeMarkers.clear();
    m_mergeCandidateMarkers.clear();
    m_bitmapTextCache.clear();
    m_sunObject.reset();
    m_moonObject.reset();
    m_rightMouseOrbiting = false;
    m_topDownView = false;
    m_gameSpeedIndex = 0;
    m_cameraYaw = 0.0f;
    m_cameraPitch = DefaultOrbitPitch;
    m_cameraShakeTimer = 0.0f;
    m_cameraShakeDuration = 0.0f;
    m_cameraShakeIntensity = 0.0f;
    m_bossIntroTimer = 0.0f;
    m_consumableCooldown = 0.0f;
    m_cameraFocus = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    m_cameraZoom = 44.0f;
    UpdateGameplayCamera(0.0f);
    m_collisionManager = make_unique<CollisionManager>();
    m_physicsManager = make_unique<PhysicsManager>();
    m_terrainCollider.reset();
    m_terrainObject.reset();
    m_spawnTimer = 0.0f;
    m_wave = 1;
    m_spawnedInWave = 0;
    m_selectedStage = clamp(m_selectedStage, 0, StagePresetCount - 1);
    if (m_selectedStage < static_cast<int>(m_stageHeightMaps.size()) &&
        m_selectedStage < static_cast<int>(m_stageTerrainMeshes.size()))
    {
        m_terrainHeightMap = m_stageHeightMaps[m_selectedStage];
        m_terrainMesh = m_stageTerrainMeshes[m_selectedStage];
    }
    else if (!m_stageHeightMaps.empty() && !m_stageTerrainMeshes.empty())
    {
        m_terrainHeightMap = m_stageHeightMaps.front();
        m_terrainMesh = m_stageTerrainMeshes.front();
    }

    m_selectedDifficulty = clamp(m_selectedDifficulty, 0, DifficultyPresetCount - 1);
    static constexpr int StartingLives[DifficultyPresetCount]{ 30, 25, 20 };
    static constexpr int StartingGold[DifficultyPresetCount]{ 18, 14, 12 };
    m_maxLives = StartingLives[m_selectedDifficulty];
    m_lives = m_maxLives;
    m_gold = StartingGold[m_selectedDifficulty];
    m_waveRunning = false;
    m_bossRewardPending = false;
    m_bossRewardWave = 0;
    m_waveRewardPending = false;
    m_waveRewardWave = 0;
    m_shopCollapsed = false;
    for (int& level : m_towerDamageLevels) level = 0;
    m_totalEnemiesDefeated = 0;
    m_bossesDefeated = 0;
    m_towersPlaced = 0;
    m_towersMerged = 0;
    m_highestTowerTier = 1;
    m_goldEarned = 0;
    m_wavesCleared = 0;
    BuildField();
}

void TowerDefenseScene::BuildField()
{
    BuildSun();
    BuildMoon();

    if (!m_terrainHeightMap || !m_terrainMesh) return;

    m_terrainObject = make_shared<GameObject>();
    m_terrainObject->SetName("Defense Terrain");
    m_terrainObject->SetMesh(m_terrainMesh);
    m_terrainObject->SetMaterial(m_fieldMaterial);
    EnableShadowCasting(m_terrainObject);

    XMFLOAT4X4 terrainWorld{};
    XMStoreFloat4x4(&terrainWorld, XMMatrixTranslation(
        -m_terrainHeightMap->GetWorldWidth() * 0.5f,
        0.0f,
        -m_terrainHeightMap->GetWorldLength() * 0.5f));
    m_terrainObject->SetWorldMatrix(terrainWorld);

    m_terrainCollider = make_shared<TerrainCollider>(m_terrainHeightMap);
    m_terrainObject->SetCollider(m_terrainCollider);
    if (m_collisionManager) m_collisionManager->AddCollider(m_terrainCollider);
    m_objects.push_back(m_terrainObject);

    BuildPath();

    if (m_enemyPaths.empty() || m_enemyPaths.front().empty()) return;

    for (size_t route = 0; route < m_enemyPaths.size(); ++route)
    {
        if (m_enemyPaths[route].empty()) continue;

        XMFLOAT3 spawn = m_enemyPaths[route].front();
        BuildTunnelMouth("Spawn Tunnel", spawn, -1.0f);
    }

    XMFLOAT3 exit = m_enemyPaths.front().back();
    BuildTunnelMouth("Exit Tunnel", exit, 1.0f);

    BuildShop();
}

void TowerDefenseScene::BuildShop()
{
    RollShopOffers(true);

    m_shopPanel = CreateCubeObject("Shop UI Panel",
        XMFLOAT3{ 0.0f, 0.0f, 0.0f },
        XMFLOAT3{ 1.0f, 1.0f, 1.0f },
        m_shopPanelMaterial);

    m_shopSlots.clear();
    for (int slot = 1; slot <= 3; ++slot)
    {
        m_shopSlots.push_back(CreateCubeObject("Shop UI Slot",
            XMFLOAT3{ 0.0f, 0.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_shopSlotMaterial));
        const TowerDefenseOffer& offer = m_shopOffers[slot - 1];
        m_shopSlots.push_back(CreateCubeObject("Shop UI Tower Icon",
            XMFLOAT3{ 0.0f, 0.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_shopTowerMaterials[TowerTypeIndex(offer.type)][offer.tier - 1]));
    }

    m_hudWidgets.clear();
    m_hudWidgets.reserve(180);
    for (int i = 0; i < 180; ++i)
    {
        m_hudWidgets.push_back(CreateCubeObject("HUD Widget",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_shopPanelMaterial));
    }

    BuildTowerInfoUI();
    BuildGoldUI();
    BuildWaveUI();
    BuildBossHealthUI();
}

void TowerDefenseScene::BuildTowerInfoUI()
{
    const size_t widgetCount = WidgetIndex(TowerInfoWidget::Count);
    m_towerInfoWidgets.assign(widgetCount, nullptr);

    auto makeWidget = [this](const string& name, const shared_ptr<Material>& material)
        {
            return CreateCubeObject(name,
                XMFLOAT3{ 0.0f, 0.0f, 0.0f },
                XMFLOAT3{ 1.0f, 1.0f, 1.0f },
                material);
        };

    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::Panel)] =
        makeWidget("Tower Info Panel", m_shopPanelMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::TowerIcon)] =
        makeWidget("Tower Info Icon", m_shopTowerMaterials[0][0]);

    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::DamageIcon)] =
        makeWidget("Tower Info Damage Icon", m_infoDamageMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::DamageTrack)] =
        makeWidget("Tower Info Damage Track", m_infoBarBackgroundMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::DamageFill)] =
        makeWidget("Tower Info Damage Fill", m_infoDamageMaterial);

    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::RangeIcon)] =
        makeWidget("Tower Info Range Icon", m_infoRangeMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::RangeTrack)] =
        makeWidget("Tower Info Range Track", m_infoBarBackgroundMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::RangeFill)] =
        makeWidget("Tower Info Range Fill", m_infoRangeMaterial);

    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::FireRateIcon)] =
        makeWidget("Tower Info Fire Rate Icon", m_infoFireRateMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::FireRateTrack)] =
        makeWidget("Tower Info Fire Rate Track", m_infoBarBackgroundMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::FireRateFill)] =
        makeWidget("Tower Info Fire Rate Fill", m_infoFireRateMaterial);

    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::CooldownIcon)] =
        makeWidget("Tower Info Cooldown Icon", m_infoCooldownMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::CooldownTrack)] =
        makeWidget("Tower Info Cooldown Track", m_infoBarBackgroundMaterial);
    m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::CooldownFill)] =
        makeWidget("Tower Info Cooldown Fill", m_infoCooldownMaterial);

    m_selectedTowerRangeMarkers.clear();
    for (int i = 0; i < 8; ++i)
    {
        m_selectedTowerRangeMarkers.push_back(CreateCubeObject("Selected Tower Range",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_scopeMaterial));
    }

    m_mergeCandidateMarkers.clear();
    m_mergeCandidateMarkers.reserve(96);
    for (int i = 0; i < 96; ++i)
    {
        m_mergeCandidateMarkers.push_back(CreateCubeObject("Merge Candidate Marker",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_mergeHighlightMaterial));
    }
}

void TowerDefenseScene::BuildGoldUI()
{
    constexpr size_t GoldWidgetCount = 3u;
    m_goldUiWidgets.assign(GoldWidgetCount, nullptr);

    auto makeWidget = [this](const string& name, const shared_ptr<Material>& material)
        {
            return CreateCubeObject(name,
                XMFLOAT3{ 0.0f, 0.0f, 0.0f },
                XMFLOAT3{ 1.0f, 1.0f, 1.0f },
                material);
        };

    m_goldUiWidgets[GoldWidgetIndex(GoldUiWidget::Panel)] =
        makeWidget("Gold UI Panel", m_shopPanelMaterial);
    m_goldUiWidgets[GoldWidgetIndex(GoldUiWidget::Coin)] =
        makeWidget("Gold UI Coin", m_goldUiMaterial);
    m_goldUiWidgets[GoldWidgetIndex(GoldUiWidget::Life)] =
        makeWidget("Life UI Icon", m_lifeUiMaterial);
}

void TowerDefenseScene::BuildWaveUI()
{
    const size_t widgetCount = WaveWidgetIndex(WaveUiWidget::Count);
    m_waveUiWidgets.assign(widgetCount, nullptr);

    auto makeWidget = [this](const string& name, const shared_ptr<Material>& material)
        {
            return CreateCubeObject(name,
                XMFLOAT3{ 0.0f, 0.0f, 0.0f },
                XMFLOAT3{ 1.0f, 1.0f, 1.0f },
                material);
        };

    m_waveUiWidgets[WaveWidgetIndex(WaveUiWidget::Panel)] =
        makeWidget("Wave Preview Panel", m_shopPanelMaterial);
}

void TowerDefenseScene::BuildBossHealthUI()
{
    const size_t widgetCount = BossWidgetIndex(BossHealthWidget::Count);
    m_bossHealthWidgets.assign(widgetCount, nullptr);

    auto makeWidget = [this](const string& name, const shared_ptr<Material>& material)
        {
            return CreateCubeObject(name,
                XMFLOAT3{ 0.0f, 0.0f, 0.0f },
                XMFLOAT3{ 1.0f, 1.0f, 1.0f },
                material);
        };

    m_bossHealthWidgets[BossWidgetIndex(BossHealthWidget::Panel)] =
        makeWidget("Boss Health Panel", m_shopPanelMaterial);
    m_bossHealthWidgets[BossWidgetIndex(BossHealthWidget::Track)] =
        makeWidget("Boss Health Track", m_infoBarBackgroundMaterial);
    m_bossHealthWidgets[BossWidgetIndex(BossHealthWidget::Fill)] =
        makeWidget("Boss Health Fill", m_bossHealthFillMaterial);
}

void TowerDefenseScene::BuildBitmapTextPool()
{
    m_bitmapTextRects.clear();
    m_bitmapTextRects.reserve(12000);
    for (int i = 0; i < 12000; ++i)
    {
        m_bitmapTextRects.push_back(CreateCubeObject("Bitmap Text Rect",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_bitmapTextMaterial));
    }
}

void TowerDefenseScene::BuildStageTerrainAssets(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    m_stageHeightMaps.clear();
    m_stageTerrainMeshes.clear();
    m_stageHeightMaps.reserve(StagePresetCount);
    m_stageTerrainMeshes.reserve(StagePresetCount);

    for (int stage = 0; stage < StagePresetCount; ++stage)
    {
        auto heightMap = CreateStageHeightMap(stage, TerrainSamples, TerrainSamples, TerrainCellSpacing);
        m_stageHeightMaps.push_back(heightMap);
        m_stageTerrainMeshes.push_back(make_shared<TerrainMesh>(device, commandList, heightMap));
    }

    m_selectedStage = clamp(m_selectedStage, 0, StagePresetCount - 1);
    if (!m_stageHeightMaps.empty())
    {
        m_terrainHeightMap = m_stageHeightMaps[m_selectedStage];
        m_terrainMesh = m_stageTerrainMeshes[m_selectedStage];
    }
}

void TowerDefenseScene::BuildTowerModelAssets(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    m_towerModelMesh.reset();
    m_towerModelParts.clear();
    for (auto& materialRow : m_towerModelMaterials)
    {
        for (auto& material : materialRow) material.reset();
    }

    filesystem::path modelDirectory = ResolveProjectAssetPath("Model");
    if (!modelDirectory.empty() && !filesystem::is_directory(modelDirectory))
    {
        modelDirectory = modelDirectory.parent_path();
    }

    auto resolveModelFile = [](const vector<filesystem::path>& candidates)
        {
            for (const filesystem::path& candidate : candidates)
            {
                filesystem::path resolved = ResolveProjectAssetPath(filesystem::path("Model") / candidate);
                if (!resolved.empty()) return resolved;
            }
            return filesystem::path{};
        };

    auto findFallbackInModelDirectory = [modelDirectory](const string& extension, bool preferRed)
        {
            if (modelDirectory.empty()) return filesystem::path{};

            filesystem::path fallback;
            try
            {
                for (const auto& entry : filesystem::directory_iterator(modelDirectory))
                {
                    if (!entry.is_regular_file()) continue;
                    const filesystem::path path = entry.path();
                    if (path.extension() != extension) continue;
                    if (path.filename().string().find("__") != string::npos) continue;

                    const string lowerName = LowerCopy(path.filename().string());
                    const bool isRed = lowerName.find("red") != string::npos;
                    if (preferRed && isRed) return filesystem::absolute(path);
                    if (fallback.empty()) fallback = filesystem::absolute(path);
                }
            }
            catch (const exception&)
            {
                return filesystem::path{};
            }

            return fallback;
        };

    filesystem::path meshPath = resolveModelFile({
        "red.bin",
        "WGX_PACK05-01-red.bin",
        "WGX_PACK05-01.bin"
        });
    if (meshPath.empty()) meshPath = findFallbackInModelDirectory(".bin", true);

    filesystem::path materialPath;
    if (!meshPath.empty())
    {
        materialPath = meshPath;
        materialPath.replace_extension(".matbin");
        if (!filesystem::exists(materialPath)) materialPath.clear();
    }
    if (materialPath.empty())
    {
        materialPath = resolveModelFile({
            "red.matbin",
            "WGX_PACK05-01-red.matbin",
            "WGX_PACK05-01.matbin"
            });
    }

    if (!meshPath.empty() && !materialPath.empty())
    {
        try
        {
            m_towerModelMesh = make_shared<BinaryMesh>(device, commandList, meshPath.string());
            for (int type = 0; type < TowerTypeCount; ++type)
            {
                const TowerDefenseTowerType towerType = TowerTypeFromIndex(type);
                for (int tier = 1; tier <= MaxTowerTier; ++tier)
                {
                    auto material = Material::CreateFromAsset(device, m_shader, materialPath.string());
                    const XMFLOAT4 tint = TowerColor(towerType, tier);
                    material->SetBaseColor(XMFLOAT4{
                        max(0.35f, tint.x),
                        max(0.35f, tint.y),
                        max(0.35f, tint.z),
                        1.0f
                    });
                    m_towerModelMaterials[type][tier - 1] = material;
                }
            }
        }
        catch (const exception& error)
        {
            OutputDebugStringA(("Tower model asset load failed: " + string(error.what()) + "\n").c_str());
            m_towerModelMesh.reset();
            for (auto& materialRow : m_towerModelMaterials)
            {
                for (auto& material : materialRow) material.reset();
            }
        }
    }

    if (modelDirectory.empty() && !meshPath.empty()) modelDirectory = meshPath.parent_path();

    filesystem::path manifestPath = resolveModelFile({
        "red.tower",
        "WGX_PACK05-01-red.tower",
        "WGX_PACK05-01.tower"
        });
    if (!modelDirectory.empty())
    {
        if (manifestPath.empty() && !meshPath.empty())
        {
            filesystem::path preferredManifest = meshPath;
            preferredManifest.replace_extension(".tower");
            if (filesystem::exists(preferredManifest)) manifestPath = preferredManifest;
        }

        if (manifestPath.empty()) manifestPath = findFallbackInModelDirectory(".tower", true);
    }

    if (manifestPath.empty()) return;

    try
    {
        vector<TowerPartManifestRecord> records;
        if (!ReadTowerModelManifest(manifestPath, records)) return;

        const filesystem::path manifestDirectory = manifestPath.parent_path();
        for (const TowerPartManifestRecord& record : records)
        {
            const filesystem::path partMeshPath = manifestDirectory / record.meshFile;
            const filesystem::path partMaterialPath = manifestDirectory / record.materialFile;
            if (!filesystem::exists(partMeshPath) || !filesystem::exists(partMaterialPath)) continue;

            TowerDefenseTowerModelPart part{};
            part.name = record.name;
            part.localMatrix = record.localMatrix;
            part.rotatesWithTarget = record.rotatesWithTarget;
            part.mesh = make_shared<BinaryMesh>(device, commandList, partMeshPath.string());
            part.materials.assign(TowerTypeCount, vector<shared_ptr<Material>>(MaxTowerTier));

            for (int type = 0; type < TowerTypeCount; ++type)
            {
                const TowerDefenseTowerType towerType = TowerTypeFromIndex(type);
                for (int tier = 1; tier <= MaxTowerTier; ++tier)
                {
                    auto material = Material::CreateFromAsset(device, m_shader, partMaterialPath.string());
                    const XMFLOAT4 tint = TowerColor(towerType, tier);
                    material->SetBaseColor(XMFLOAT4{
                        max(0.35f, tint.x),
                        max(0.35f, tint.y),
                        max(0.35f, tint.z),
                        1.0f
                    });
                    part.materials[type][tier - 1] = material;
                }
            }

            m_towerModelParts.push_back(std::move(part));
        }

        const bool hasRotatingPart = any_of(m_towerModelParts.begin(), m_towerModelParts.end(),
            [](const TowerDefenseTowerModelPart& part) { return part.rotatesWithTarget; });
        if (!hasRotatingPart && m_towerModelParts.size() > 1)
        {
            auto highestPart = max_element(m_towerModelParts.begin(), m_towerModelParts.end(),
                [](const TowerDefenseTowerModelPart& lhs, const TowerDefenseTowerModelPart& rhs)
                {
                    return lhs.localMatrix._42 < rhs.localMatrix._42;
                });
            if (highestPart != m_towerModelParts.end()) highestPart->rotatesWithTarget = true;
        }
    }
    catch (const exception& error)
    {
        OutputDebugStringA(("Tower hierarchy asset load failed: " + string(error.what()) + "\n").c_str());
        m_towerModelParts.clear();
    }
}

XMFLOAT4X4 TowerDefenseScene::BuildTowerModelRootMatrix(const XMFLOAT3& terrainPoint,
    TowerDefenseTowerType type,
    int tier) const
{
    tier = clamp(tier, 1, MaxTowerTier);
    const float terrainHeight = TerrainHeightAtWorldXZ(terrainPoint.x, terrainPoint.z);

    XMFLOAT4X4 world{};
    if (!m_towerModelMesh)
    {
        XMStoreFloat4x4(&world, XMMatrixTranslation(
            terrainPoint.x,
            terrainHeight + TowerHalfHeight,
            terrainPoint.z));
        return world;
    }

    const BoundingBox bounds = m_towerModelMesh->GetLocalAABB();
    const XMFLOAT3 targetSize = TowerVisualSize(type, tier);
    const float localHeight = max(0.001f, bounds.Extents.y * 2.0f);
    const float localFootprint = max(0.001f, max(bounds.Extents.x, bounds.Extents.z) * 2.0f);
    const float targetHeight = max(0.30f, targetSize.y * 2.0f);
    const float targetFootprint = max(0.30f, max(targetSize.x, targetSize.z) * 2.35f);
    const float modelScale = min(targetHeight / localHeight, targetFootprint / localFootprint);
    const float localBottom = bounds.Center.y - bounds.Extents.y;

    XMStoreFloat4x4(&world,
        XMMatrixScaling(modelScale, modelScale, modelScale) *
        XMMatrixTranslation(
            terrainPoint.x,
            terrainHeight - localBottom * modelScale,
            terrainPoint.z));
    return world;
}

XMFLOAT3 TowerDefenseScene::GetTowerProjectileOrigin(const TowerDefenseTower& tower) const
{
    XMFLOAT3 origin = tower.object ? tower.object->GetPosition() : tower.position;
    float highestY = origin.y;

    for (const TowerDefenseTowerPartInstance& instance : tower.parts)
    {
        if (!instance.object) continue;

        const XMFLOAT3 partPosition = instance.object->GetPosition();
        if (instance.rotatesWithTarget)
        {
            origin = partPosition;
            highestY = partPosition.y;
            break;
        }

        if (partPosition.y > highestY)
        {
            origin = partPosition;
            highestY = partPosition.y;
        }
    }

    origin.y += TowerHalfHeight * 0.62f;
    return origin;
}

void TowerDefenseScene::LoadBitmapFont()
{
    m_bitmapFontPath = ResolveBitmapFontPath();
    if (m_bitmapFontPath.empty()) return;

    m_bitmapFontLoaded = AddFontResourceExW(
        m_bitmapFontPath.c_str(),
        FR_PRIVATE,
        nullptr) > 0;
}

void TowerDefenseScene::ReleaseBitmapFont()
{
    if (!m_bitmapFontLoaded || m_bitmapFontPath.empty()) return;

    RemoveFontResourceExW(m_bitmapFontPath.c_str(), FR_PRIVATE, nullptr);
    m_bitmapFontLoaded = false;
}

void TowerDefenseScene::RollShopOffers(bool freeReroll)
{
    if (m_mode == TowerDefenseMode::Playing && !freeReroll)
    {
        const int rerollCost = GetRerollCost();
        if (m_gold < rerollCost) return;
        m_gold -= rerollCost;
    }

    for (auto& offer : m_shopOffers)
    {
        offer = CreateRandomOffer();
    }

    ClearDragGhost();
}

TowerDefenseOffer TowerDefenseScene::CreateRandomOffer() const
{
    const float roll = Utiles::Random::GetFloat(0.0f, 1.0f);
    int tier = 1;
    if (roll > 0.96f) tier = 3;
    else if (roll > 0.74f) tier = 2;

    const float kindRoll = Utiles::Random::GetFloat(0.0f, 1.0f);
    TowerDefenseOffer offer{};
    offer.tier = tier;
    if (kindRoll < 0.10f)
    {
        offer.kind = TowerDefenseOfferKind::Generator;
        offer.type = TowerDefenseTowerType::Basic;
        offer.cost = 7 + tier * 4;
        return offer;
    }
    if (kindRoll < 0.17f)
    {
        offer.kind = TowerDefenseOfferKind::Meteor;
        offer.type = TowerDefenseTowerType::Splash;
        offer.cost = 4 + tier * 3 + max(0, m_wave - 1) / 2;
        return offer;
    }
    if (kindRoll < 0.25f)
    {
        offer.kind = TowerDefenseOfferKind::Freeze;
        offer.type = TowerDefenseTowerType::Slow;
        offer.cost = 3 + tier * 2 + max(0, m_wave - 1) / 3;
        return offer;
    }
    if (kindRoll < 0.33f)
    {
        offer.kind = TowerDefenseOfferKind::Boost;
        offer.type = TowerDefenseTowerType::Rapid;
        offer.cost = 3 + tier * 2;
        return offer;
    }
    if (kindRoll < 0.41f)
    {
        offer.kind = TowerDefenseOfferKind::Boulder;
        offer.type = TowerDefenseTowerType::Splash;
        offer.cost = 5 + tier * 3 + max(0, m_wave - 1) / 3;
        return offer;
    }

    const int typeIndex = Utiles::Random::GetInt(0, TowerTypeCount - 1);
    offer.kind = TowerDefenseOfferKind::Tower;
    offer.type = TowerTypeFromIndex(typeIndex);
    offer.cost = 2 + tier * 2;
    if (offer.type == TowerDefenseTowerType::Splash) ++offer.cost;
    if (offer.type == TowerDefenseTowerType::Slow) ++offer.cost;
    if (offer.type == TowerDefenseTowerType::Mortar) offer.cost += 2;
    if (offer.type == TowerDefenseTowerType::Flak) ++offer.cost;
    return offer;
}

void TowerDefenseScene::RenderShopUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_camera || !g_framework) return;
    if (m_bossRewardPending || m_waveRewardPending) return;

    const float width = static_cast<float>(g_framework->GetWindowWidth());
    const float height = static_cast<float>(g_framework->GetWindowHeight());
    if (width <= 1.0f || height <= 1.0f) return;

    if (m_shopPanel)
    {
        const XMFLOAT2 buttonCenter = m_shopCollapsed
            ? XMFLOAT2{ 0.5f, 0.945f }
            : XMFLOAT2{ 0.5f, 0.760f };
        const XMFLOAT2 buttonSize{ 0.110f, 0.045f };
        m_shopPanel->SetMaterial(m_shopCollapsed ? m_goldUiMaterial : m_shopSlotMaterial);
        XMFLOAT4X4 buttonWorld = BuildCameraAnchoredUiMatrix(buttonCenter, buttonSize, 4.12f, 0.032f);
        m_shopPanel->SetWorldMatrix(buttonWorld);
        m_shopPanel->Render(commandList);
        RenderBitmapText(commandList,
            m_shopCollapsed ? L"SHOP" : L"HIDE",
            XMFLOAT2{ buttonCenter.x, buttonCenter.y - 0.012f },
            0.026f,
            3.72f,
            m_bitmapTextMaterial,
            0.5f);
    }

    if (m_shopCollapsed) return;

    if (m_shopPanel)
    {
        m_shopPanel->SetMaterial(m_shopPanelMaterial);
        XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(
            XMFLOAT2{ 0.5f, 0.865f },
            XMFLOAT2{ 0.38f, 0.16f },
            4.2f,
            0.025f);
        m_shopPanel->SetWorldMatrix(world);
        m_shopPanel->Render(commandList);
    }

    size_t objectIndex = 0;
    size_t markerWidgetIndex = 96;
    for (int slot = 1; slot <= 3; ++slot)
    {
        XMFLOAT2 slotCenter{};
        XMFLOAT2 slotHalfSize{};
        if (!GetShopSlotRect(slot, width, height, slotCenter, slotHalfSize)) continue;

        const XMFLOAT2 normalizedCenter{
            slotCenter.x / width,
            slotCenter.y / height
        };
        const XMFLOAT2 normalizedSlotSize{
            (slotHalfSize.x * 2.0f) / width,
            (slotHalfSize.y * 2.0f) / height
        };

        if (objectIndex < m_shopSlots.size() && m_shopSlots[objectIndex])
        {
            const TowerDefenseOffer& offer = m_shopOffers[slot - 1];
            const shared_ptr<Material>& slotMaterial =
                offer.kind == TowerDefenseOfferKind::Generator ? m_shopGeneratorSlotMaterial :
                IsConsumableOffer(offer.kind) ? m_shopConsumableSlotMaterial :
                m_shopSlotMaterial;
            XMFLOAT4X4 slotWorld = BuildCameraAnchoredUiMatrix(
                normalizedCenter,
                normalizedSlotSize,
                4.05f,
                0.030f);
            m_shopSlots[objectIndex]->SetMesh(m_cube);
            m_shopSlots[objectIndex]->SetMaterial(slotMaterial);
            m_shopSlots[objectIndex]->SetWorldMatrix(slotWorld);
            m_shopSlots[objectIndex]->Render(commandList);
        }
        ++objectIndex;

        if (objectIndex < m_shopSlots.size() && m_shopSlots[objectIndex])
        {
            const TowerDefenseOffer& offer = m_shopOffers[slot - 1];
            const int offerType = TowerTypeIndex(offer.type);
            const int offerTier = clamp(offer.tier, 1, MaxTowerTier);
            const float iconScale = offer.kind != TowerDefenseOfferKind::Tower
                ? 0.64f + static_cast<float>(offerTier) * 0.08f
                : 0.56f + static_cast<float>(offerTier) * 0.10f;
            if (offer.kind == TowerDefenseOfferKind::Tower && m_towerModelMesh)
            {
                RenderTowerModelIcon(commandList,
                    m_shopSlots[objectIndex],
                    offer.type,
                    offerTier,
                    XMFLOAT2{ normalizedCenter.x, normalizedCenter.y - normalizedSlotSize.y * 0.03f },
                    normalizedSlotSize.y * iconScale * 0.92f,
                    3.86f);
            }
            else
            {
                XMFLOAT4X4 iconWorld = BuildCameraAnchoredUiMatrix(
                    normalizedCenter,
                    XMFLOAT2{ normalizedSlotSize.x * iconScale, normalizedSlotSize.y * iconScale },
                    3.92f,
                    0.12f);
                const shared_ptr<Material>& iconMaterial =
                    offer.kind == TowerDefenseOfferKind::Meteor ? m_meteorUiMaterial :
                    offer.kind == TowerDefenseOfferKind::Freeze ? m_freezeUiMaterial :
                    offer.kind == TowerDefenseOfferKind::Boost ? m_boostUiMaterial :
                    offer.kind == TowerDefenseOfferKind::Boulder ? m_boulderUiMaterial :
                    offer.kind == TowerDefenseOfferKind::Generator ? m_generatorUiMaterial :
                    m_shopTowerMaterials[offerType][offerTier - 1];
                m_shopSlots[objectIndex]->SetMesh(m_cube);
                m_shopSlots[objectIndex]->SetMaterial(iconMaterial);
                m_shopSlots[objectIndex]->SetWorldMatrix(iconWorld);
                m_shopSlots[objectIndex]->Render(commandList);
            }

            WCHAR costText[16]{};
            swprintf_s(costText, L"%dC", offer.cost);
            RenderBitmapText(commandList,
                costText,
                XMFLOAT2{ normalizedCenter.x, normalizedCenter.y + normalizedSlotSize.y * 0.25f },
                0.025f,
                3.72f,
                m_gold >= offer.cost ? m_goldDigitMaterial : m_lifeUiMaterial,
                0.5f);

            RenderTierMarkers(commandList,
                XMFLOAT2{ normalizedCenter.x, normalizedCenter.y - normalizedSlotSize.y * 0.43f },
                offerTier,
                0.0065f,
                0.019f,
                3.68f,
                markerWidgetIndex);

            RenderBitmapText(commandList,
                OfferShortName(offer),
                XMFLOAT2{ normalizedCenter.x, normalizedCenter.y - normalizedSlotSize.y * 0.12f },
                0.023f,
                3.70f,
                m_bitmapTextMaterial,
                0.5f);
        }
        ++objectIndex;
    }

    WCHAR rerollText[32]{};
    swprintf_s(rerollText, L"R ROLL %dC", GetRerollCost());
    RenderBitmapText(commandList,
        rerollText,
        XMFLOAT2{ 0.5f, 0.928f },
        0.030f,
        3.72f,
        m_bitmapTextMaterial,
        0.5f);

    WCHAR itemCooldownText[32]{};
    if (m_consumableCooldown > 0.0f)
    {
        swprintf_s(itemCooldownText, L"ITEM CD %.1f", m_consumableCooldown);
    }
    else
    {
        swprintf_s(itemCooldownText, L"ITEM READY");
    }
    RenderBitmapText(commandList,
        itemCooldownText,
        XMFLOAT2{ 0.5f, 0.956f },
        0.022f,
        3.72f,
        m_consumableCooldown > 0.0f ? m_bossHealthFillMaterial : m_boostUiMaterial,
        0.5f);
}

void TowerDefenseScene::RenderSelectedTowerRange(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    const TowerDefenseTower* tower = GetSelectedTower();
    if (!tower || !tower->object || m_selectedTowerRangeMarkers.size() < 4) return;

    const XMFLOAT3 position = tower->object->GetPosition();
    const float y = TerrainHeightAtWorldXZ(position.x, position.z) + 0.055f;
    constexpr float Thickness = 0.035f;

    auto renderRangeBox = [&](size_t startIndex,
        float radius,
        float heightOffset,
        const shared_ptr<Material>& material,
        float thickness)
        {
            if (radius <= 0.05f || startIndex + 4 > m_selectedTowerRangeMarkers.size()) return;

            const float markerY = y + heightOffset;
            const XMFLOAT3 centers[4] = {
                XMFLOAT3{ position.x - radius, markerY, position.z },
                XMFLOAT3{ position.x + radius, markerY, position.z },
                XMFLOAT3{ position.x, markerY, position.z - radius },
                XMFLOAT3{ position.x, markerY, position.z + radius }
            };
            const XMFLOAT3 scales[4] = {
                XMFLOAT3{ thickness, thickness, radius },
                XMFLOAT3{ thickness, thickness, radius },
                XMFLOAT3{ radius, thickness, thickness },
                XMFLOAT3{ radius, thickness, thickness }
            };

            for (size_t i = 0; i < 4; ++i)
            {
                auto& marker = m_selectedTowerRangeMarkers[startIndex + i];
                if (!marker) continue;
                marker->SetMaterial(material);

                XMFLOAT4X4 world{};
                XMStoreFloat4x4(&world, XMMatrixScaling(scales[i].x, scales[i].y, scales[i].z) *
                    XMMatrixTranslation(centers[i].x, centers[i].y, centers[i].z));
                marker->SetWorldMatrix(world);
                marker->Render(commandList);
            }
        };

    renderRangeBox(0, max(0.25f, tower->range), 0.0f, m_scopeMaterial, Thickness);
    if (tower->minRange > 0.05f)
    {
        renderRangeBox(4, tower->minRange, 0.035f, m_deathEffectMaterial, Thickness * 1.20f);
    }
}

void TowerDefenseScene::RenderMergeCandidateHighlights(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mergeCandidateMarkers.empty()) return;

    TowerDefenseTowerType mergeType = TowerDefenseTowerType::Basic;
    int mergeTier = 0;
    shared_ptr<GameObject> sourceObject;

    if (m_draggingTower && m_dragOffer.kind == TowerDefenseOfferKind::Tower)
    {
        mergeType = m_dragOffer.type;
        mergeTier = m_dragOffer.tier;
        sourceObject = m_dragSourceTower.lock();
    }
    else
    {
        const TowerDefenseTower* selectedTower = GetSelectedTower();
        sourceObject = m_selectedTower.lock();
        if (!selectedTower || !sourceObject) return;

        mergeType = selectedTower->type;
        mergeTier = selectedTower->tier;
    }

    if (mergeTier <= 0 || mergeTier >= MaxTowerTier) return;

    size_t markerIndex = 0;
    constexpr float Thickness = 0.045f;
    for (const auto& tower : m_towers)
    {
        if (!tower.object || tower.object == sourceObject) continue;
        if (tower.type != mergeType || tower.tier != mergeTier) continue;
        if (markerIndex + 4 > m_mergeCandidateMarkers.size()) return;

        const XMFLOAT3 position = tower.object->GetPosition();
        const float y = TerrainHeightAtWorldXZ(position.x, position.z) + 0.10f;
        const float radius = 0.72f + static_cast<float>(tower.tier) * 0.18f;
        const XMFLOAT3 centers[4] = {
            XMFLOAT3{ position.x - radius, y, position.z },
            XMFLOAT3{ position.x + radius, y, position.z },
            XMFLOAT3{ position.x, y, position.z - radius },
            XMFLOAT3{ position.x, y, position.z + radius }
        };
        const XMFLOAT3 scales[4] = {
            XMFLOAT3{ Thickness, Thickness, radius },
            XMFLOAT3{ Thickness, Thickness, radius },
            XMFLOAT3{ radius, Thickness, Thickness },
            XMFLOAT3{ radius, Thickness, Thickness }
        };

        for (int i = 0; i < 4; ++i)
        {
            auto& marker = m_mergeCandidateMarkers[markerIndex++];
            if (!marker) continue;

            XMFLOAT4X4 world{};
            XMStoreFloat4x4(&world, XMMatrixScaling(scales[i].x, scales[i].y, scales[i].z) *
                XMMatrixTranslation(centers[i].x, centers[i].y, centers[i].z));
            marker->SetWorldMatrix(world);
            marker->Render(commandList);
        }
    }
}

void TowerDefenseScene::RenderTowerInfoUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    const TowerDefenseTower* tower = GetSelectedTower();
    if (!tower || !m_camera || !g_framework) return;
    if (m_towerInfoWidgets.size() < WidgetIndex(TowerInfoWidget::Count)) return;

    const float width = static_cast<float>(g_framework->GetWindowWidth());
    const float height = static_cast<float>(g_framework->GetWindowHeight());
    if (width <= 1.0f || height <= 1.0f) return;

    auto renderWidget = [this, &commandList](TowerInfoWidget widget,
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            const size_t index = WidgetIndex(widget);
            if (index >= m_towerInfoWidgets.size() || !m_towerInfoWidgets[index]) return;
            m_towerInfoWidgets[index]->SetMesh(m_cube);
            if (material) m_towerInfoWidgets[index]->SetMaterial(material);

            XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, thickness);
            m_towerInfoWidgets[index]->SetWorldMatrix(world);
            m_towerInfoWidgets[index]->Render(commandList);
        };

    renderWidget(TowerInfoWidget::Panel,
        XMFLOAT2{ 0.835f, 0.150f },
        XMFLOAT2{ 0.290f, 0.235f },
        4.20f,
        0.025f,
        m_shopPanelMaterial);

    const int tier = clamp(tower->tier, 1, 3);
    const int typeIndex = TowerTypeIndex(tower->type);
    const float iconScale = 0.048f + static_cast<float>(tier) * 0.007f;
    if (m_towerModelMesh)
    {
        RenderTowerModelIcon(commandList,
            m_towerInfoWidgets[WidgetIndex(TowerInfoWidget::TowerIcon)],
            tower->type,
            tier,
            XMFLOAT2{ 0.724f, 0.073f },
            iconScale * 1.70f,
            3.86f);
    }
    else
    {
        renderWidget(TowerInfoWidget::TowerIcon,
            XMFLOAT2{ 0.728f, 0.075f },
            XMFLOAT2{ iconScale, iconScale * 1.18f },
            3.92f,
            0.12f,
            m_shopTowerMaterials[typeIndex][tier - 1]);
    }

    size_t tierMarkerWidgetIndex = 132;
    RenderTierMarkers(commandList,
        XMFLOAT2{ 0.765f, 0.058f },
        tier,
        0.0058f,
        0.017f,
        3.68f,
        tierMarkerWidgetIndex);
    RenderBitmapText(commandList,
        TowerTypeShortName(tower->type),
        XMFLOAT2{ 0.745f, 0.074f },
        0.020f,
        3.78f,
        m_bitmapTextMaterial);

    const XMFLOAT2 barSize{ 0.170f, 0.017f };
    const float barCenterX = 0.850f;
    const float iconX = 0.730f;
    const float firstRowY = 0.112f;
    const float rowStep = 0.040f;

    auto renderBar = [&](int row,
        TowerInfoWidget icon,
        TowerInfoWidget track,
        TowerInfoWidget fill,
        float ratio,
        const shared_ptr<Material>& material)
        {
            const float y = firstRowY + static_cast<float>(row) * rowStep;
            renderWidget(icon,
                XMFLOAT2{ iconX, y },
                XMFLOAT2{ 0.014f, 0.014f },
                3.91f,
                0.030f,
                material);
            renderWidget(track,
                XMFLOAT2{ barCenterX, y },
                barSize,
                4.03f,
                0.018f,
                m_infoBarBackgroundMaterial);

            ratio = clamp(ratio, 0.0f, 1.0f);
            if (ratio <= 0.01f) return;

            const float fillWidth = barSize.x * ratio;
            const float barLeft = barCenterX - barSize.x * 0.5f;
            renderWidget(fill,
                XMFLOAT2{ barLeft + fillWidth * 0.5f, y },
                XMFLOAT2{ fillWidth, barSize.y },
                3.93f,
                0.020f,
                material);
        };

    const float attackRate = 1.0f / max(tower->fireInterval, 0.001f);
    const float cooldownReady = 1.0f - clamp(tower->cooldown / max(tower->fireInterval, 0.001f), 0.0f, 1.0f);

    renderBar(0, TowerInfoWidget::DamageIcon, TowerInfoWidget::DamageTrack, TowerInfoWidget::DamageFill,
        tower->damage / 92.0f, m_infoDamageMaterial);
    renderBar(1, TowerInfoWidget::RangeIcon, TowerInfoWidget::RangeTrack, TowerInfoWidget::RangeFill,
        tower->range / 11.50f, m_infoRangeMaterial);
    renderBar(2, TowerInfoWidget::FireRateIcon, TowerInfoWidget::FireRateTrack, TowerInfoWidget::FireRateFill,
        attackRate / 4.40f, m_infoFireRateMaterial);
    renderBar(3, TowerInfoWidget::CooldownIcon, TowerInfoWidget::CooldownTrack, TowerInfoWidget::CooldownFill,
        cooldownReady, m_infoCooldownMaterial);

    WCHAR textBuffer[32]{};
    swprintf_s(textBuffer, L"ATK %.0f", tower->damage);
    RenderBitmapText(commandList, textBuffer, XMFLOAT2{ 0.745f, 0.102f }, 0.024f, 3.78f, m_bitmapTextMaterial);
    swprintf_s(textBuffer, L"RANGE %.1f", tower->range);
    RenderBitmapText(commandList, textBuffer, XMFLOAT2{ 0.745f, 0.142f }, 0.024f, 3.78f, m_bitmapTextMaterial);
    swprintf_s(textBuffer, L"SPD %.1f", attackRate);
    RenderBitmapText(commandList, textBuffer, XMFLOAT2{ 0.745f, 0.182f }, 0.024f, 3.78f, m_bitmapTextMaterial);
    swprintf_s(textBuffer, L"CD %.0f%%", cooldownReady * 100.0f);
    RenderBitmapText(commandList, textBuffer, XMFLOAT2{ 0.745f, 0.222f }, 0.024f, 3.78f, m_bitmapTextMaterial);
    if (tower->minRange > 0.05f)
    {
        WCHAR minText[32]{};
        swprintf_s(minText, L"MIN %.1f", tower->minRange);
        RenderBitmapText(commandList, minText, XMFLOAT2{ 0.745f, 0.252f }, 0.020f, 3.78f, m_goldDigitMaterial);
        RenderBitmapText(commandList, L"FAR ONLY", XMFLOAT2{ 0.835f, 0.252f }, 0.020f, 3.78f, m_bossHealthFillMaterial);
    }
    else if (tier < MaxTowerTier)
    {
        RenderBitmapText(commandList, L"MERGE SAME", XMFLOAT2{ 0.745f, 0.252f }, 0.020f, 3.78f, m_goldDigitMaterial);
    }
    else
    {
        RenderBitmapText(commandList, L"MAX TIER", XMFLOAT2{ 0.745f, 0.252f }, 0.020f, 3.78f, m_goldDigitMaterial);
    }
}

void TowerDefenseScene::BeginBitmapTextPass() const
{
    m_bitmapTextCursor = 0;
}

void TowerDefenseScene::RenderBitmapText(const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const wstring& text,
    const XMFLOAT2& anchor,
    float normalizedHeight,
    float depth,
    const shared_ptr<Material>& material,
    float alignX) const
{
    if (text.empty() || !m_camera || !g_framework || m_bitmapTextRects.empty()) return;

    const TowerDefenseTextCache& cache = GetBitmapTextCache(text, 24);
    if (cache.runs.empty()) return;

    const float aspect = max(0.1f, g_framework->GetAspectRatio());
    const float unitY = max(0.0001f, normalizedHeight) / max(1.0f, cache.height);
    const float unitX = unitY / aspect;
    const float fullWidth = cache.width * unitX;
    const float startX = anchor.x - fullWidth * clamp(alignX, 0.0f, 1.0f);
    const float startY = anchor.y;
    const bool drawShadow = m_infoBarBackgroundMaterial != nullptr && material != m_infoBarBackgroundMaterial;

    for (const auto& run : cache.runs)
    {
        if (m_bitmapTextCursor >= m_bitmapTextRects.size()) return;

        const XMFLOAT2 center{
            startX + run.center.x * unitX,
            startY + run.center.y * unitY
        };
        const XMFLOAT2 size{
            max(unitX, run.size.x * unitX),
            max(unitY, run.size.y * unitY)
        };

        if (drawShadow)
        {
            auto& shadow = m_bitmapTextRects[m_bitmapTextCursor++];
            if (shadow)
            {
                shadow->SetMaterial(m_infoBarBackgroundMaterial);
                const XMFLOAT2 shadowCenter{
                    center.x + unitX * 1.5f,
                    center.y + unitY * 1.6f
                };
                XMFLOAT4X4 shadowWorld = BuildCameraAnchoredUiMatrix(shadowCenter, size, depth + 0.018f, 0.010f);
                shadow->SetWorldMatrix(shadowWorld);
                shadow->Render(commandList);
            }
            if (m_bitmapTextCursor >= m_bitmapTextRects.size()) return;
        }

        auto& rect = m_bitmapTextRects[m_bitmapTextCursor++];
        if (!rect) continue;

        if (material) rect->SetMaterial(material);
        XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, 0.010f);
        rect->SetWorldMatrix(world);
        rect->Render(commandList);
    }
}

void TowerDefenseScene::RenderStartScreenUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::StartScreen) return;

    size_t widgetIndex = 0;
    auto renderWidget = [this, &commandList, &widgetIndex](
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            if (widgetIndex >= m_hudWidgets.size() || !m_hudWidgets[widgetIndex]) return;
            auto& widget = m_hudWidgets[widgetIndex++];
            widget->SetMesh(m_cube);
            if (material) widget->SetMaterial(material);
            widget->SetWorldMatrix(BuildCameraAnchoredUiMatrix(center, size, depth, thickness));
            widget->Render(commandList);
        };

    renderWidget(XMFLOAT2{ 0.5f, 0.530f }, XMFLOAT2{ 0.570f, 0.455f }, 4.16f, 0.032f, m_shopPanelMaterial);
    renderWidget(XMFLOAT2{ 0.5f, 0.805f }, XMFLOAT2{ 0.220f, 0.070f }, 3.96f, 0.040f, m_startMaterial);

    RenderBitmapText(commandList,
        L"TOP DEFENSE",
        XMFLOAT2{ 0.5f, 0.315f },
        0.056f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);

    RenderBitmapText(commandList,
        L"STAGE",
        XMFLOAT2{ 0.5f, 0.394f },
        0.030f,
        3.70f,
        m_goldDigitMaterial,
        0.5f);

    const float stageCenters[StagePresetCount]{ 0.345f, 0.500f, 0.655f };
    for (int stage = 0; stage < StagePresetCount; ++stage)
    {
        const bool selected = stage == clamp(m_selectedStage, 0, StagePresetCount - 1);
        renderWidget(
            XMFLOAT2{ stageCenters[stage], 0.448f },
            XMFLOAT2{ 0.132f, 0.062f },
            selected ? 3.90f : 4.01f,
            0.032f,
            selected ? m_startMaterial : m_shopSlotMaterial);
        RenderBitmapText(commandList,
            StagePresetName(stage),
            XMFLOAT2{ stageCenters[stage], 0.435f },
            0.022f,
            3.66f,
            selected ? m_bitmapTextMaterial : m_goldDigitMaterial,
            0.5f);
    }

    RenderBitmapText(commandList,
        L"DIFFICULTY",
        XMFLOAT2{ 0.5f, 0.526f },
        0.030f,
        3.70f,
        m_goldDigitMaterial,
        0.5f);

    const float difficultyCenters[DifficultyPresetCount]{ 0.345f, 0.500f, 0.655f };
    for (int difficulty = 0; difficulty < DifficultyPresetCount; ++difficulty)
    {
        const bool selected = difficulty == clamp(m_selectedDifficulty, 0, DifficultyPresetCount - 1);
        const shared_ptr<Material>& material = selected
            ? (difficulty == 2 ? m_bossHealthFillMaterial : difficulty == 0 ? m_infoRangeMaterial : m_startMaterial)
            : m_shopSlotMaterial;
        renderWidget(
            XMFLOAT2{ difficultyCenters[difficulty], 0.580f },
            XMFLOAT2{ 0.132f, 0.062f },
            selected ? 3.90f : 4.01f,
            0.032f,
            material);
        RenderBitmapText(commandList,
            DifficultyPresetName(difficulty),
            XMFLOAT2{ difficultyCenters[difficulty], 0.567f },
            0.021f,
            3.66f,
            m_bitmapTextMaterial,
            0.5f);
    }

    WCHAR ruleText[96]{};
    swprintf_s(ruleText,
        L"HP %.0f%%  SPD %.0f%%  REWARD %.0f%%",
        GetDifficultyHealthMultiplier() * 100.0f,
        GetDifficultySpeedMultiplier() * 100.0f,
        GetDifficultyRewardMultiplier() * 100.0f);
    RenderBitmapText(commandList,
        ruleText,
        XMFLOAT2{ 0.5f, 0.650f },
        0.022f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);

    RenderBitmapText(commandList,
        L"START",
        XMFLOAT2{ 0.5f, 0.790f },
        0.040f,
        3.64f,
        m_bitmapTextMaterial,
        0.5f);

    RenderBitmapText(commandList,
        L"DRAG TOWER  R ROLL  V VIEW  F SPEED",
        XMFLOAT2{ 0.5f, 0.868f },
        0.024f,
        3.70f,
        m_goldDigitMaterial,
        0.5f);
}

void TowerDefenseScene::RenderGoldUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode == TowerDefenseMode::StartScreen || !m_camera || !g_framework) return;
    if (m_goldUiWidgets.size() < 3u) return;

    auto renderWidget = [this, &commandList](size_t index,
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            if (index >= m_goldUiWidgets.size() || !m_goldUiWidgets[index]) return;
            if (material) m_goldUiWidgets[index]->SetMaterial(material);

            XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, thickness);
            m_goldUiWidgets[index]->SetWorldMatrix(world);
            m_goldUiWidgets[index]->Render(commandList);
        };

    renderWidget(GoldWidgetIndex(GoldUiWidget::Panel),
        XMFLOAT2{ 0.500f, 0.026f },
        XMFLOAT2{ 1.040f, 0.056f },
        4.18f,
        0.018f,
        m_shopPanelMaterial);

    renderWidget(GoldWidgetIndex(GoldUiWidget::Coin),
        XMFLOAT2{ 0.884f, 0.030f },
        XMFLOAT2{ 0.022f, 0.022f },
        3.92f,
        0.050f,
        m_goldUiMaterial);

    renderWidget(GoldWidgetIndex(GoldUiWidget::Life),
        XMFLOAT2{ 0.028f, 0.030f },
        XMFLOAT2{ 0.024f, 0.024f },
        3.92f,
        0.050f,
        m_lifeUiMaterial);

    WCHAR goldText[32]{};
    swprintf_s(goldText, L"%d C", max(0, m_gold));
    RenderBitmapText(commandList, goldText, XMFLOAT2{ 0.972f, 0.014f }, 0.045f, 3.78f, m_goldDigitMaterial, 1.0f);

    WCHAR lifeText[32]{};
    swprintf_s(lifeText, L"%d/%d", max(0, m_lives), max(1, m_maxLives));
    RenderBitmapText(commandList, lifeText, XMFLOAT2{ 0.052f, 0.014f }, 0.045f, 3.78f, m_lifeUiMaterial);
}

void TowerDefenseScene::RenderWavePreviewUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_camera || !g_framework) return;
    if (m_waveUiWidgets.size() < WaveWidgetIndex(WaveUiWidget::Count)) return;

    auto renderWidget = [this, &commandList](WaveUiWidget widget,
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            const size_t index = WaveWidgetIndex(widget);
            if (index >= m_waveUiWidgets.size() || !m_waveUiWidgets[index]) return;
            if (material) m_waveUiWidgets[index]->SetMaterial(material);

            XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, thickness);
            m_waveUiWidgets[index]->SetWorldMatrix(world);
            m_waveUiWidgets[index]->Render(commandList);
        };

    renderWidget(WaveUiWidget::Panel,
        XMFLOAT2{ 0.500f, 0.032f },
        XMFLOAT2{ 0.270f, 0.050f },
        4.18f,
        0.014f,
        m_shopPanelMaterial);

    renderWidget(WaveUiWidget::Panel,
        XMFLOAT2{ 0.500f, 0.083f },
        XMFLOAT2{ 0.105f, 0.042f },
        4.16f,
        0.022f,
        m_waveRunning ? m_bossHealthFillMaterial : m_goldUiMaterial);

    const int waveSize = GetWaveSize(m_wave);
    const int remainingToSpawn = max(0, waveSize - m_spawnedInWave);
    WCHAR waveText[64]{};
    swprintf_s(waveText, L"WAVE %d-%d", m_wave, MaxWave);
    RenderBitmapText(commandList,
        waveText,
        XMFLOAT2{ 0.500f, 0.008f },
        0.045f,
        3.78f,
        m_bitmapTextMaterial,
        0.5f);

    WCHAR previewText[64]{};
    if (GetActiveBoss())
    {
        swprintf_s(previewText, L"BOSS LIVE");
    }
    else if (IsBossWave(m_wave) && remainingToSpawn > 0)
    {
        swprintf_s(previewText, L"BOSS SOON");
    }
    else if (m_wave < MaxWave)
    {
        const int nextWave = m_wave + 1;
        swprintf_s(previewText,
            IsBossWave(nextWave) ? L"NEXT W%d BOSS" : L"NEXT W%d %d ENM",
            nextWave,
            GetWaveSize(nextWave));
    }
    else
    {
        swprintf_s(previewText, L"FINAL WAVE");
    }

    RenderBitmapText(commandList,
        previewText,
        XMFLOAT2{ 0.500f, 0.116f },
        0.018f,
        3.78f,
        IsBossWave(m_wave) || GetActiveBoss() ? m_bossHealthFillMaterial : m_goldDigitMaterial,
        0.5f);

    const wchar_t* toggleText = (m_bossRewardPending || m_waveRewardPending) ? L"PICK" : (m_waveRunning ? L"STOP" : L"START");
    RenderBitmapText(commandList,
        toggleText,
        XMFLOAT2{ 0.500f, 0.071f },
        0.026f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);

    WCHAR speedText[16]{};
    swprintf_s(speedText, L"F x%.0f", GetGameSpeedMultiplier());
    RenderBitmapText(commandList,
        speedText,
        XMFLOAT2{ 0.590f, 0.071f },
        0.020f,
        3.70f,
        m_goldDigitMaterial,
        0.5f);
}

void TowerDefenseScene::RenderTowerUpgradeUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_camera || m_hudWidgets.empty()) return;
    if (m_bossRewardPending || m_waveRewardPending) return;

    size_t widgetIndex = 0;
    auto renderWidget = [this, &commandList, &widgetIndex](
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            if (widgetIndex >= m_hudWidgets.size() || !m_hudWidgets[widgetIndex]) return;
            auto& widget = m_hudWidgets[widgetIndex++];
            widget->SetMesh(m_cube);
            if (material) widget->SetMaterial(material);

            XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, thickness);
            widget->SetWorldMatrix(world);
            widget->Render(commandList);
        };

    renderWidget(XMFLOAT2{ 0.132f, 0.835f }, XMFLOAT2{ 0.245f, 0.285f }, 4.14f, 0.024f, m_shopPanelMaterial);
    RenderBitmapText(commandList,
        L"DAMAGE UP",
        XMFLOAT2{ 0.132f, 0.710f },
        0.027f,
        3.72f,
        m_bitmapTextMaterial,
        0.5f);

    for (int type = 0; type < TowerTypeCount; ++type)
    {
        const int level = clamp(m_towerDamageLevels[type], 0, MaxTowerDamageUpgradeLevel);
        const int cost = GetTowerDamageUpgradeCost(type);
        const bool maxed = level >= MaxTowerDamageUpgradeLevel;
        const bool affordable = !maxed && m_gold >= cost;
        const int column = type % 3;
        const int row = type / 3;
        const float x = 0.059f + static_cast<float>(column) * 0.073f;
        const float y = 0.785f + static_cast<float>(row) * 0.108f;
        const TowerDefenseTowerType towerType = TowerTypeFromIndex(type);

        renderWidget(
            XMFLOAT2{ x, y },
            XMFLOAT2{ 0.060f, 0.060f },
            4.02f,
            0.022f,
            affordable ? m_shopTowerMaterials[type][0] : m_infoBarBackgroundMaterial);

        if (m_towerModelMesh && widgetIndex < m_hudWidgets.size() && m_hudWidgets[widgetIndex])
        {
            RenderTowerModelIcon(commandList,
                m_hudWidgets[widgetIndex++],
                towerType,
                1,
                XMFLOAT2{ x, y - 0.012f },
                0.047f,
                3.84f);
        }
        else
        {
            renderWidget(
                XMFLOAT2{ x, y - 0.010f },
                XMFLOAT2{ 0.022f, 0.022f },
                3.90f,
                0.026f,
                m_shopTowerMaterials[type][0]);
        }

        RenderBitmapText(commandList,
            TowerTypeShortName(towerType),
            XMFLOAT2{ x, y - 0.039f },
            0.014f,
            3.70f,
            m_bitmapTextMaterial,
            0.5f);

        WCHAR levelText[16]{};
        swprintf_s(levelText, L"LV%d", level);
        RenderBitmapText(commandList,
            levelText,
            XMFLOAT2{ x, y + 0.017f },
            0.017f,
            3.70f,
            affordable || maxed ? m_goldDigitMaterial : m_lifeUiMaterial,
            0.5f);

        WCHAR costText[16]{};
        swprintf_s(costText, maxed ? L"MAX" : L"%dC", cost);
        RenderBitmapText(commandList,
            costText,
            XMFLOAT2{ x, y + 0.041f },
            0.014f,
            3.70f,
            affordable ? m_goldDigitMaterial : m_lifeUiMaterial,
            0.5f);
    }
}

void TowerDefenseScene::RenderMiniMapUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_camera || !m_terrainHeightMap || m_hudWidgets.empty()) return;
    if (m_bossRewardPending || m_waveRewardPending) return;

    size_t widgetIndex = 42;
    auto renderWidget = [this, &commandList, &widgetIndex](
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            if (widgetIndex >= m_hudWidgets.size() || !m_hudWidgets[widgetIndex]) return;
            auto& widget = m_hudWidgets[widgetIndex++];
            widget->SetMesh(m_cube);
            if (material) widget->SetMaterial(material);

            XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, thickness);
            widget->SetWorldMatrix(world);
            widget->Render(commandList);
        };

    const XMFLOAT2 panelCenter{ 0.890f, 0.887f };
    const XMFLOAT2 panelSize{ 0.205f, 0.220f };
    const XMFLOAT2 mapCenter{ 0.890f, 0.902f };
    const XMFLOAT2 mapSize{ 0.170f, 0.158f };
    renderWidget(panelCenter, panelSize, 4.14f, 0.024f, m_shopPanelMaterial);
    renderWidget(mapCenter, mapSize, 4.02f, 0.018f, m_infoBarBackgroundMaterial);

    RenderBitmapText(commandList,
        L"MINI MAP",
        XMFLOAT2{ panelCenter.x, 0.790f },
        0.023f,
        3.72f,
        m_bitmapTextMaterial,
        0.5f);

    const float terrainWidth = max(1.0f, m_terrainHeightMap->GetWorldWidth());
    const float terrainLength = max(1.0f, m_terrainHeightMap->GetWorldLength());
    auto mapPoint = [&](const XMFLOAT3& position)
        {
            const float nx = clamp(position.x / terrainWidth + 0.5f, 0.0f, 1.0f);
            const float nz = clamp(position.z / terrainLength + 0.5f, 0.0f, 1.0f);
            return XMFLOAT2{
                mapCenter.x - mapSize.x * 0.5f + nx * mapSize.x,
                mapCenter.y + mapSize.y * 0.5f - nz * mapSize.y
            };
        };

    for (const auto& path : m_enemyPaths)
    {
        for (const XMFLOAT3& point : path)
        {
            renderWidget(mapPoint(point), XMFLOAT2{ 0.0055f, 0.0055f }, 3.90f, 0.010f, m_infoRangeMaterial);
        }
    }

    for (const auto& tower : m_towers)
    {
        if (!tower.object) continue;
        const int typeIndex = TowerTypeIndex(tower.type);
        renderWidget(mapPoint(tower.object->GetPosition()),
            XMFLOAT2{ 0.0070f, 0.0070f },
            3.88f,
            0.012f,
            m_shopTowerMaterials[typeIndex][clamp(tower.tier, 1, MaxTowerTier) - 1]);
    }

    for (const auto& enemy : m_enemies)
    {
        if (!enemy.object || enemy.health <= 0.0f) continue;
        const shared_ptr<Material>& material = enemy.isBoss
            ? m_bossHealthFillMaterial
            : enemy.isFlying ? m_freezeUiMaterial : m_lifeUiMaterial;
        const float size = enemy.isBoss ? 0.0120f : 0.0065f;
        renderWidget(mapPoint(enemy.object->GetPosition()), XMFLOAT2{ size, size }, 3.86f, 0.012f, material);
    }
}

void TowerDefenseScene::RenderBossHealthUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    const TowerDefenseEnemy* boss = GetActiveBoss();
    if (!boss || !m_camera || !g_framework) return;
    if (m_bossHealthWidgets.size() < BossWidgetIndex(BossHealthWidget::Count)) return;

    auto renderWidget = [this, &commandList](BossHealthWidget widget,
        const XMFLOAT2& center,
        const XMFLOAT2& size,
        float depth,
        float thickness,
        const shared_ptr<Material>& material)
        {
            const size_t index = BossWidgetIndex(widget);
            if (index >= m_bossHealthWidgets.size() || !m_bossHealthWidgets[index]) return;
            if (material) m_bossHealthWidgets[index]->SetMaterial(material);

            XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, thickness);
            m_bossHealthWidgets[index]->SetWorldMatrix(world);
            m_bossHealthWidgets[index]->Render(commandList);
        };

    renderWidget(BossHealthWidget::Panel,
        XMFLOAT2{ 0.455f, 0.160f },
        XMFLOAT2{ 0.325f, 0.072f },
        4.17f,
        0.025f,
        m_shopPanelMaterial);

    const XMFLOAT2 TrackCenter{ 0.455f, 0.166f };
    const XMFLOAT2 TrackSize{ 0.265f, 0.020f };
    renderWidget(BossHealthWidget::Track,
        TrackCenter,
        TrackSize,
        4.02f,
        0.016f,
        m_infoBarBackgroundMaterial);

    const float ratio = boss->maxHealth > 0.0f ? clamp(boss->health / boss->maxHealth, 0.0f, 1.0f) : 0.0f;
    const float fillWidth = max(0.001f, TrackSize.x * ratio);
    const float left = TrackCenter.x - TrackSize.x * 0.5f;
    renderWidget(BossHealthWidget::Fill,
        XMFLOAT2{ left + fillWidth * 0.5f, TrackCenter.y },
        XMFLOAT2{ fillWidth, TrackSize.y },
        3.92f,
        0.018f,
        m_bossHealthFillMaterial);

    WCHAR bossText[48]{};
    swprintf_s(bossText, L"BOSS %.0f%%", ratio * 100.0f);
    RenderBitmapText(commandList,
        bossText,
        XMFLOAT2{ 0.325f, 0.126f },
        0.028f,
        3.78f,
        m_bitmapTextMaterial);
}

void TowerDefenseScene::RenderBossIntroUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || m_bossIntroTimer <= 0.0f || !m_camera || !m_shopPanel) return;

    const float pulse = 0.5f + 0.5f * sinf(m_bossIntroTimer * 12.0f);
    m_shopPanel->SetMaterial(m_bossHealthFillMaterial);
    XMFLOAT4X4 panelWorld = BuildCameraAnchoredUiMatrix(
        XMFLOAT2{ 0.5f, 0.315f },
        XMFLOAT2{ 0.50f + pulse * 0.030f, 0.125f + pulse * 0.010f },
        4.02f,
        0.042f);
    m_shopPanel->SetWorldMatrix(panelWorld);
    m_shopPanel->Render(commandList);

    RenderBitmapText(commandList,
        L"BOSS INCOMING",
        XMFLOAT2{ 0.5f, 0.280f },
        0.060f + pulse * 0.006f,
        3.68f,
        m_bitmapTextMaterial,
        0.5f);
    RenderBitmapText(commandList,
        L"LONG RANGE READY",
        XMFLOAT2{ 0.5f, 0.352f },
        0.028f,
        3.68f,
        m_goldDigitMaterial,
        0.5f);
}

void TowerDefenseScene::RenderBossRewardUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_bossRewardPending || !m_camera || !m_shopPanel) return;
    if (m_shopSlots.size() < 3) return;

    m_shopPanel->SetMaterial(m_shopPanelMaterial);
    XMFLOAT4X4 panelWorld = BuildCameraAnchoredUiMatrix(
        XMFLOAT2{ 0.5f, 0.442f },
        XMFLOAT2{ 0.660f, 0.320f },
        4.01f,
        0.055f);
    m_shopPanel->SetWorldMatrix(panelWorld);
    m_shopPanel->Render(commandList);

    RenderBitmapText(commandList,
        L"BOSS REWARD",
        XMFLOAT2{ 0.5f, 0.304f },
        0.054f,
        3.66f,
        m_bitmapTextMaterial,
        0.5f);

    const XMFLOAT2 choiceCenters[3]{
        XMFLOAT2{ 0.330f, 0.462f },
        XMFLOAT2{ 0.500f, 0.462f },
        XMFLOAT2{ 0.670f, 0.462f }
    };
    WCHAR goldDetail[24]{};
    swprintf_s(goldDetail, L"+%dC", ScaleReward(14 + max(1, m_bossRewardWave) * 3));
    const wchar_t* labels[3]{ L"GOLD", L"LIFE", L"SHOP" };
    const wchar_t* detail[3]{ goldDetail, L"+5", L"UP" };
    const shared_ptr<Material> materials[3]{
        m_goldUiMaterial,
        m_lifeUiMaterial,
        m_shopGeneratorSlotMaterial
    };

    for (int i = 0; i < 3; ++i)
    {
        if (!m_shopSlots[i]) continue;

        m_shopSlots[i]->SetMesh(m_cube);
        m_shopSlots[i]->SetMaterial(materials[i] ? materials[i] : m_shopSlotMaterial);
        XMFLOAT4X4 choiceWorld = BuildCameraAnchoredUiMatrix(
            choiceCenters[i],
            XMFLOAT2{ 0.148f, 0.126f },
            3.88f,
            0.050f);
        m_shopSlots[i]->SetWorldMatrix(choiceWorld);
        m_shopSlots[i]->Render(commandList);

        RenderBitmapText(commandList,
            labels[i],
            XMFLOAT2{ choiceCenters[i].x, choiceCenters[i].y - 0.036f },
            0.034f,
            3.62f,
            m_bitmapTextMaterial,
            0.5f);
        RenderBitmapText(commandList,
            detail[i],
            XMFLOAT2{ choiceCenters[i].x, choiceCenters[i].y + 0.033f },
            0.027f,
            3.62f,
            m_goldDigitMaterial,
            0.5f);
    }
}

void TowerDefenseScene::RenderWaveRewardUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_waveRewardPending || !m_camera || !m_shopPanel) return;
    if (m_shopSlots.size() < 3) return;

    m_shopPanel->SetMaterial(m_shopPanelMaterial);
    XMFLOAT4X4 panelWorld = BuildCameraAnchoredUiMatrix(
        XMFLOAT2{ 0.5f, 0.442f },
        XMFLOAT2{ 0.660f, 0.320f },
        4.00f,
        0.055f);
    m_shopPanel->SetWorldMatrix(panelWorld);
    m_shopPanel->Render(commandList);

    WCHAR title[48]{};
    swprintf_s(title, L"WAVE %d CLEAR", max(1, m_waveRewardWave));
    RenderBitmapText(commandList,
        title,
        XMFLOAT2{ 0.5f, 0.304f },
        0.052f,
        3.66f,
        m_bitmapTextMaterial,
        0.5f);

    const int goldBonus = ScaleReward(6 + max(1, m_waveRewardWave) * 2);
    WCHAR goldDetail[24]{};
    swprintf_s(goldDetail, L"+%dC", goldBonus);

    const XMFLOAT2 choiceCenters[3]{
        XMFLOAT2{ 0.330f, 0.462f },
        XMFLOAT2{ 0.500f, 0.462f },
        XMFLOAT2{ 0.670f, 0.462f }
    };
    const wchar_t* labels[3]{ L"GOLD", L"LIFE", L"SHOP" };
    const wchar_t* detail[3]{ goldDetail, L"+2", L"FREE" };
    const shared_ptr<Material> materials[3]{
        m_goldUiMaterial,
        m_lifeUiMaterial,
        m_shopGeneratorSlotMaterial
    };

    for (int i = 0; i < 3; ++i)
    {
        if (!m_shopSlots[i]) continue;

        m_shopSlots[i]->SetMesh(m_cube);
        m_shopSlots[i]->SetMaterial(materials[i] ? materials[i] : m_shopSlotMaterial);
        XMFLOAT4X4 choiceWorld = BuildCameraAnchoredUiMatrix(
            choiceCenters[i],
            XMFLOAT2{ 0.148f, 0.126f },
            3.87f,
            0.050f);
        m_shopSlots[i]->SetWorldMatrix(choiceWorld);
        m_shopSlots[i]->Render(commandList);

        RenderBitmapText(commandList,
            labels[i],
            XMFLOAT2{ choiceCenters[i].x, choiceCenters[i].y - 0.036f },
            0.034f,
            3.62f,
            m_bitmapTextMaterial,
            0.5f);
        RenderBitmapText(commandList,
            detail[i],
            XMFLOAT2{ choiceCenters[i].x, choiceCenters[i].y + 0.033f },
            0.027f,
            3.62f,
            m_goldDigitMaterial,
            0.5f);
    }
}

void TowerDefenseScene::RenderDamagePopups(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (!m_camera || !g_framework) return;

    for (const auto& popup : m_damagePopups)
    {
        if (popup.text.empty() || popup.maxLifetime <= 0.0f) continue;

        XMFLOAT2 screen{};
        float depth = 0.0f;
        if (!WorldToScreenUi(popup.position, screen, depth)) continue;

        const float t = clamp(popup.lifetime / popup.maxLifetime, 0.0f, 1.0f);
        const int typeIndex = TowerTypeIndex(popup.type);
        const shared_ptr<Material>& material =
            popup.type == TowerDefenseTowerType::Slow ? m_infoRangeMaterial :
            popup.type == TowerDefenseTowerType::Mortar ? m_meteorUiMaterial :
            popup.type == TowerDefenseTowerType::Flak ? m_freezeUiMaterial :
            m_hitMaterials[typeIndex] ? m_hitMaterials[typeIndex] : m_goldDigitMaterial;

        RenderBitmapText(commandList,
            popup.text,
            screen,
            popup.size * (0.82f + t * 0.28f),
            max(3.55f, 3.86f - depth * 0.012f),
            material,
            0.5f);
    }
}

void TowerDefenseScene::RenderResultUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if ((m_mode != TowerDefenseMode::Victory && m_mode != TowerDefenseMode::Defeat) ||
        !m_camera || !m_shopPanel) return;

    const shared_ptr<Material>& resultMaterial =
        m_mode == TowerDefenseMode::Victory ? m_resultVictoryMaterial : m_resultDefeatMaterial;

    m_shopPanel->SetMaterial(resultMaterial);
    XMFLOAT4X4 panelWorld = BuildCameraAnchoredUiMatrix(
        XMFLOAT2{ 0.5f, 0.45f },
        XMFLOAT2{ 0.50f, 0.285f },
        4.05f,
        0.055f);
    m_shopPanel->SetWorldMatrix(panelWorld);
    m_shopPanel->Render(commandList);

    const int accentCount = min(static_cast<int>(m_shopSlots.size()), 6);
    for (int i = 0; i < accentCount; ++i)
    {
        if (!m_shopSlots[i]) continue;

        const float x = 0.5f + (static_cast<float>(i) - 2.5f) * 0.045f;
        const float y = m_mode == TowerDefenseMode::Victory
            ? 0.405f + (i % 2) * 0.065f
            : 0.450f;
        m_shopSlots[i]->SetMesh(m_cube);
        m_shopSlots[i]->SetMaterial(resultMaterial);
        XMFLOAT4X4 accentWorld = BuildCameraAnchoredUiMatrix(
            XMFLOAT2{ x, y },
            XMFLOAT2{ 0.026f, 0.026f },
            3.88f,
            0.040f);
        m_shopSlots[i]->SetWorldMatrix(accentWorld);
        m_shopSlots[i]->Render(commandList);
    }

    RenderBitmapText(commandList,
        m_mode == TowerDefenseMode::Victory ? L"VICTORY" : L"DEFEAT",
        XMFLOAT2{ 0.5f, 0.345f },
        0.066f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);

    WCHAR lineA[72]{};
    swprintf_s(lineA, L"KILL %d   BOSS %d", m_totalEnemiesDefeated, m_bossesDefeated);
    RenderBitmapText(commandList,
        lineA,
        XMFLOAT2{ 0.5f, 0.424f },
        0.030f,
        3.70f,
        m_goldDigitMaterial,
        0.5f);

    WCHAR lineB[72]{};
    swprintf_s(lineB, L"WAVE %d/%d   GOLD +%d", m_wavesCleared, MaxWave, m_goldEarned);
    RenderBitmapText(commandList,
        lineB,
        XMFLOAT2{ 0.5f, 0.466f },
        0.030f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);

    WCHAR lineC[72]{};
    swprintf_s(lineC, L"PLACE %d   MERGE %d   BEST", m_towersPlaced, m_towersMerged);
    RenderBitmapText(commandList,
        lineC,
        XMFLOAT2{ 0.472f, 0.508f },
        0.028f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);
    size_t tierMarkerWidgetIndex = 154;
    RenderTierMarkers(commandList,
        XMFLOAT2{ 0.626f, 0.508f },
        m_highestTowerTier,
        0.0065f,
        0.019f,
        3.68f,
        tierMarkerWidgetIndex);

    RenderBitmapText(commandList,
        L"ESC MENU",
        XMFLOAT2{ 0.5f, 0.560f },
        0.030f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);
}

void TowerDefenseScene::UpdateGameplayCamera(float timeElapsed)
{
    if (m_debugCameraEnabled || !m_camera) return;

    auto spectatorCamera = dynamic_pointer_cast<SpectatorCamera>(m_camera);
    if (!spectatorCamera) return;

    m_cameraFocus = ClampCameraFocusToTerrain(m_cameraFocus);
    XMFLOAT3 focus = m_cameraFocus;
    focus.y += 0.85f;

    XMFLOAT3 desiredEye{};
    if (m_topDownView)
    {
        const float topHeight = max(38.0f, m_cameraZoom * 1.18f);
        desiredEye = XMFLOAT3{ focus.x, focus.y + topHeight, focus.z - 0.05f };
    }
    else
    {
        const float armDistance = max(18.0f, m_cameraZoom);
        const float horizontalDistance = cosf(m_cameraPitch) * armDistance;
        const float verticalDistance = sinf(m_cameraPitch) * armDistance;
        desiredEye = XMFLOAT3{
            focus.x + sinf(m_cameraYaw) * horizontalDistance,
            focus.y + verticalDistance,
            focus.z - cosf(m_cameraYaw) * horizontalDistance
        };
        desiredEye = ApplySpringCameraCollision(focus, desiredEye);
    }

    if (m_cameraShakeTimer > 0.0f && m_cameraShakeDuration > 0.0f)
    {
        m_cameraShakeTimer = max(0.0f, m_cameraShakeTimer - timeElapsed);
        const float ratio = clamp(m_cameraShakeTimer / m_cameraShakeDuration, 0.0f, 1.0f);
        const float strength = m_cameraShakeIntensity * ratio * ratio;
        const float phase = m_cameraShakeTimer * 71.0f;
        XMFLOAT3 right = m_topDownView
            ? XMFLOAT3{ 1.0f, 0.0f, 0.0f }
            : Normalize(XMFLOAT3{ cosf(m_cameraYaw), 0.0f, sinf(m_cameraYaw) });
        XMFLOAT3 forward = m_topDownView
            ? XMFLOAT3{ 0.0f, 0.0f, 1.0f }
            : Normalize(XMFLOAT3{ sinf(m_cameraYaw), 0.0f, -cosf(m_cameraYaw) });
        XMFLOAT3 shake = Utiles::Vector3::Add(
            Utiles::Vector3::Mul(right, sinf(phase) * strength),
            Utiles::Vector3::Mul(forward, cosf(phase * 0.73f) * strength * 0.45f));
        shake.y = sinf(phase * 1.37f) * strength * 0.32f;

        desiredEye = Utiles::Vector3::Add(desiredEye, shake);
        focus = Utiles::Vector3::Add(focus, Utiles::Vector3::Mul(shake, 0.35f));
    }

    spectatorCamera->SetPose(desiredEye, Normalize(Utiles::Vector3::Sub(focus, desiredEye)));
}

void TowerDefenseScene::UpdateCelestialCycle(float timeElapsed)
{
    m_sunCycleTime += timeElapsed * 0.0075f;
    if (m_sunCycleTime > 1.0f) m_sunCycleTime -= floorf(m_sunCycleTime);

    const float angle = m_sunCycleTime * XM_2PI;
    constexpr float SunOrbitRadius = 68.0f;
    constexpr float MaxSunIntensity = 190.0f;
    constexpr float MaxMoonIntensity = 24.0f;

    XMFLOAT3 sunPosition{
        cosf(angle) * SunOrbitRadius,
        sinf(angle) * SunOrbitRadius,
        0.0f
    };
    const float moonAngle = -angle;
    XMFLOAT3 moonPosition{
        cosf(moonAngle) * SunOrbitRadius,
        sinf(moonAngle) * SunOrbitRadius,
        0.0f
    };

    const float daylight = clamp((sunPosition.y + 8.0f) / (SunOrbitRadius * 0.72f), 0.0f, 1.0f);
    const float moonlight = clamp((moonPosition.y + 8.0f) / (SunOrbitRadius * 0.72f), 0.0f, 1.0f);
    if (m_sunLight)
    {
        m_sunLight->SetPosition(sunPosition);
        m_sunLight->SetIntensity(MaxSunIntensity * daylight);
    }
    if (m_moonLight)
    {
        m_moonLight->SetPosition(moonPosition);
        m_moonLight->SetIntensity(MaxMoonIntensity * moonlight);
    }
    if (m_shadowMap)
    {
        const float moonShadow = moonlight * 0.34f;
        if (moonShadow > daylight)
        {
            m_shadowMap->SetLight(moonPosition, XMFLOAT3{ 0.0f, 0.0f, 0.0f }, 88.0f, moonShadow);
        }
        else
        {
            m_shadowMap->SetLight(sunPosition, XMFLOAT3{ 0.0f, 0.0f, 0.0f }, 88.0f, max(daylight, 0.08f));
        }
    }
    if (m_sunObject) m_sunObject->SetPosition(sunPosition);
    if (m_moonObject) m_moonObject->SetPosition(moonPosition);
}

void TowerDefenseScene::BuildSun()
{
    m_sunObject = CreateCubeObject("Sun",
        XMFLOAT3{ 68.0f, 0.0f, 0.0f },
        XMFLOAT3{ 0.52f, 0.52f, 0.52f },
        m_sunMaterial);
    m_objects.push_back(m_sunObject);
    UpdateCelestialCycle(0.0f);
}

void TowerDefenseScene::BuildMoon()
{
    m_moonObject = CreateCubeObject("Moon",
        XMFLOAT3{ 68.0f, 0.0f, 0.0f },
        XMFLOAT3{ 0.34f, 0.34f, 0.34f },
        m_moonMaterial);
    m_objects.push_back(m_moonObject);
    UpdateCelestialCycle(0.0f);
}

void TowerDefenseScene::BuildTunnelMouth(const string& name, const XMFLOAT3& center, float directionSign)
{
    const float sign = directionSign < 0.0f ? -1.0f : 1.0f;
    const float groundY = TerrainHeightAtWorldXZ(center.x, center.z);
    const float mouthX = center.x + sign * 0.38f;
    const float mouthY = groundY + 0.74f;

    auto addPart = [this](const string& partName,
        const XMFLOAT3& position,
        const XMFLOAT3& scale,
        const shared_ptr<Material>& material)
        {
            auto part = CreateCubeObject(partName, position, scale, material);
            EnableShadowCasting(part);
            m_objects.push_back(part);
        };

    addPart(name + " Opening",
        XMFLOAT3{ mouthX, mouthY, center.z },
        XMFLOAT3{ 0.10f, 0.64f, 0.58f },
        m_tunnelOpeningMaterial);

    addPart(name + " Back Shadow",
        XMFLOAT3{ mouthX + sign * 0.16f, mouthY, center.z },
        XMFLOAT3{ 0.22f, 0.52f, 0.48f },
        m_tunnelOpeningMaterial);

    addPart(name + " Left Stone",
        XMFLOAT3{ mouthX, groundY + 0.62f, center.z - 0.70f },
        XMFLOAT3{ 0.20f, 0.68f, 0.14f },
        m_tunnelStoneMaterial);

    addPart(name + " Right Stone",
        XMFLOAT3{ mouthX, groundY + 0.62f, center.z + 0.70f },
        XMFLOAT3{ 0.20f, 0.68f, 0.14f },
        m_tunnelStoneMaterial);

    addPart(name + " Top Stone",
        XMFLOAT3{ mouthX, groundY + 1.34f, center.z },
        XMFLOAT3{ 0.22f, 0.18f, 0.88f },
        m_tunnelStoneMaterial);

    addPart(name + " Ground Lip",
        XMFLOAT3{ mouthX - sign * 0.08f, groundY + 0.13f, center.z },
        XMFLOAT3{ 0.28f, 0.12f, 0.78f },
        m_tunnelStoneMaterial);
}

void TowerDefenseScene::Update(FLOAT timeElapsed)
{
    m_bossIntroTimer = max(0.0f, m_bossIntroTimer - timeElapsed);
    m_consumableCooldown = max(0.0f, m_consumableCooldown - timeElapsed);
    UpdateCelestialCycle(timeElapsed);
    UpdateGameplayCamera(timeElapsed);

    if (m_mode != TowerDefenseMode::Playing)
    {
        UpdateHitMarkers(timeElapsed);
        UpdateDamagePopups(timeElapsed);
        UpdateRollingBoulders(timeElapsed);
        UpdateScopeMarkers(timeElapsed);
        return;
    }

    const float gameplayTimeElapsed = timeElapsed * GetGameSpeedMultiplier();
    UpdateEnemies(gameplayTimeElapsed);
    UpdateTowers(gameplayTimeElapsed);
    UpdateGenerators(gameplayTimeElapsed);
    UpdateProjectiles(gameplayTimeElapsed);
    UpdateHitMarkers(gameplayTimeElapsed);
    UpdateDamagePopups(gameplayTimeElapsed);
    UpdateRollingBoulders(gameplayTimeElapsed);
    UpdateScopeMarkers(gameplayTimeElapsed);

    if (m_lives <= 0)
    {
        m_mode = TowerDefenseMode::Defeat;
        ClearDragGhost();
        return;
    }

    if (m_bossRewardPending || m_waveRewardPending)
    {
        return;
    }

    const int waveSize = GetWaveSize(m_wave);
    if (m_spawnedInWave < waveSize)
    {
        if (m_waveRunning)
        {
            m_spawnTimer -= gameplayTimeElapsed;
            if (m_spawnTimer <= 0.0f)
            {
                const int remaining = waveSize - m_spawnedInWave;
                int batchCount = 1;
                const float burstRoll = Utiles::Random::GetFloat(0.0f, 1.0f);
                if (m_wave >= 2 && burstRoll < 0.16f) batchCount = min(4, remaining);
                else if (burstRoll < 0.42f) batchCount = min(2, remaining);
                if (IsBossWave(m_wave))
                {
                    batchCount = remaining <= 1 ? 1 : min(batchCount, remaining - 1);
                }

                for (int i = 0; i < batchCount && m_spawnedInWave < waveSize; ++i)
                {
                    const bool spawnBoss = IsBossWave(m_wave) && m_spawnedInWave == waveSize - 1;
                const float largeChance = min(0.10f + static_cast<float>(m_wave) * 0.018f, 0.26f);
                const bool isLarge = m_wave >= 2 && Utiles::Random::GetFloat(0.0f, 1.0f) < largeChance;
                const float variantRoll = Utiles::Random::GetFloat(0.0f, 1.0f);
                const float flyingChance = min(0.12f + static_cast<float>(m_wave) * 0.035f, 0.34f);
                const float laneOffset = batchCount > 1
                    ? (static_cast<float>(i) - (static_cast<float>(batchCount) - 1.0f) * 0.5f) * 0.46f
                    : 0.0f;

                if (spawnBoss)
                {
                    SpawnEnemy(1.0f, 1.0f, 2.70f, 0.0f, true, false, false, TowerDefenseEnemyVariant::Boss);
                }
                else if (m_wave >= 3 && variantRoll < flyingChance * 0.38f)
                {
                    SpawnEnemy(1.18f, Utiles::Random::GetFloat(0.94f, 1.08f), 1.10f, laneOffset, false, true, true, TowerDefenseEnemyVariant::FlyingSplitter);
                }
                else if (m_wave >= 2 && variantRoll < flyingChance)
                {
                    SpawnEnemy(1.0f, Utiles::Random::GetFloat(1.02f, 1.16f), 0.96f, laneOffset, false, true, false, TowerDefenseEnemyVariant::FlyingScout);
                }
                else if (m_wave >= 3 && variantRoll < flyingChance + 0.16f)
                {
                    SpawnEnemy(1.45f, 0.86f, 1.28f, laneOffset, false, false, false, TowerDefenseEnemyVariant::Armored);
                }
                else if (variantRoll < flyingChance + 0.34f)
                {
                    SpawnEnemy(0.78f, Utiles::Random::GetFloat(1.12f, 1.26f), 0.82f, laneOffset, false, false, false, TowerDefenseEnemyVariant::Runner);
                }
                else if (isLarge)
                {
                    SpawnEnemy(2.8f, 0.70f, 1.65f, laneOffset, false, false, false, TowerDefenseEnemyVariant::Brute);
                }
                else
                {
                    SpawnEnemy(1.0f, Utiles::Random::GetFloat(0.94f, 1.08f), 1.0f, laneOffset, false, false, false, TowerDefenseEnemyVariant::Walker);
                }

                    ++m_spawnedInWave;
                }

                const float minInterval = max(0.30f, 0.92f - static_cast<float>(m_wave) * 0.035f);
                const float maxInterval = max(0.52f, 1.26f - static_cast<float>(m_wave) * 0.030f);
                m_spawnTimer = Utiles::Random::GetFloat(minInterval, maxInterval);
                if (batchCount > 1) m_spawnTimer *= 1.22f;
            }
        }
    }
    else if (m_enemies.empty())
    {
        m_wavesCleared = max(m_wavesCleared, m_wave);
        if (m_wave >= MaxWave)
        {
            m_mode = TowerDefenseMode::Victory;
            AddGold(ScaleReward(10));
            ClearDragGhost();
            return;
        }

        ShowWaveReward();
    }
}

void TowerDefenseScene::UpdateEnemies(float timeElapsed)
{
    auto getPath = [this](const TowerDefenseEnemy& enemy) -> const vector<XMFLOAT3>&
        {
            if (enemy.routeIndex < m_enemyPaths.size()) return m_enemyPaths[enemy.routeIndex];
            return m_waypoints;
        };

    for (auto& enemy : m_enemies)
    {
        const auto& path = getPath(enemy);
        if (!enemy.object || path.empty() || enemy.waypointIndex >= path.size()) continue;

        enemy.slowTimer = max(0.0f, enemy.slowTimer - timeElapsed);
        if (enemy.slowTimer <= 0.0f) enemy.slowMultiplier = 1.0f;

        XMFLOAT3 position = enemy.object->GetPosition();
        XMFLOAT3 target = path[enemy.waypointIndex];
        target.y = TerrainHeightAtWorldXZ(target.x, target.z) + enemy.heightOffset;

        XMFLOAT3 planarDelta{ target.x - position.x, 0.0f, target.z - position.z };
        XMFLOAT3 velocity{ 0.0f, 0.0f, 0.0f };
        if (Utiles::Vector3::Dot(planarDelta, planarDelta) > 0.0025f)
        {
            XMFLOAT3 direction = Normalize(planarDelta);
            velocity = Utiles::Vector3::Mul(direction, enemy.speed * enemy.slowMultiplier);
        }

        if (auto rigidbody = enemy.object->GetRigidbody())
        {
            rigidbody->SetVelocity(velocity);
        }
        else
        {
            XMFLOAT3 next = MoveTowards(position, target, enemy.speed * timeElapsed);
            enemy.object->SetPosition(next);
        }
    }

    if (m_physicsManager) m_physicsManager->Update(timeElapsed);
    if (m_collisionManager) m_collisionManager->Update(false);

    for (auto& enemy : m_enemies)
    {
        const auto& path = getPath(enemy);
        if (!enemy.object || path.empty() || enemy.waypointIndex >= path.size()) continue;

        XMFLOAT3 position = enemy.object->GetPosition();
        position.y = TerrainHeightAtWorldXZ(position.x, position.z) + enemy.heightOffset;
        enemy.object->SetPosition(position);

        XMFLOAT3 target = path[enemy.waypointIndex];
        target.y = position.y;

        if (DistanceSqXZ(position, target) <= 0.09f)
        {
            ++enemy.waypointIndex;
        }
    }

    UpdateEnemyHealthBars();

    vector<pair<TowerDefenseEnemy, XMFLOAT3>> splitRequests;
    for (auto it = m_enemies.begin(); it != m_enemies.end();)
    {
        const auto& path = getPath(*it);
        const bool escaped = path.empty() || it->waypointIndex >= path.size();
        const bool defeated = it->health <= 0.0f;
        if (escaped || defeated)
        {
            if (escaped) m_lives = max(0, m_lives - 1);
            if (defeated)
            {
                const int coinReward = ScaleReward(it->isBoss
                    ? 10 + m_wave * 2
                    : 1 + (it->maxHealth > 90.0f ? 1 : 0));
                AddGold(coinReward);
                ++m_totalEnemiesDefeated;
                if (it->isBoss)
                {
                    ++m_bossesDefeated;
                    ShowBossReward();
                }
                if (it->object)
                {
                    const XMFLOAT3 deathPosition = it->object->GetPosition();
                    if (it->splitsOnDeath && it->splitCount > 0)
                    {
                        splitRequests.push_back({ *it, deathPosition });
                    }
                    const float burstScale = max(1.0f, it->visualScale) *
                        (it->isBoss ? 1.35f : 1.0f) *
                        (it->splitsOnDeath ? 1.65f : 1.0f);
                    SpawnDeathEffect(deathPosition, burstScale);
                    SpawnCoinDropEffect(deathPosition, coinReward);
                }
            }
            RemoveRenderObject(it->healthBarBack);
            RemoveRenderObject(it->healthBarFill);
            RemoveSimulationObject(it->object);
            it = m_enemies.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& request : splitRequests)
    {
        SpawnSplitChildren(request.first, request.second);
    }
}

void TowerDefenseScene::UpdateEnemyHealthBars()
{
    for (auto& enemy : m_enemies)
    {
        if (!enemy.object || !enemy.healthBarBack || !enemy.healthBarFill) continue;

        const XMFLOAT3 position = enemy.object->GetPosition();
        const float visualScale = max(0.85f, enemy.visualScale);
        const float ratio = enemy.maxHealth > 0.0f ? clamp(enemy.health / enemy.maxHealth, 0.0f, 1.0f) : 0.0f;
        const float y = position.y + 0.52f * visualScale;
        const float halfWidth = 0.28f * visualScale;
        constexpr float BarHalfHeight = 0.025f;

        XMFLOAT4X4 backWorld{};
        XMStoreFloat4x4(&backWorld, XMMatrixScaling(halfWidth, BarHalfHeight, BarHalfHeight) *
            XMMatrixTranslation(position.x, y, position.z));
        enemy.healthBarBack->SetWorldMatrix(backWorld);

        const float fillHalfWidth = max(0.001f, halfWidth * ratio);
        const float left = position.x - halfWidth;
        XMFLOAT4X4 fillWorld{};
        XMStoreFloat4x4(&fillWorld, XMMatrixScaling(fillHalfWidth, BarHalfHeight * 1.12f, BarHalfHeight * 1.12f) *
            XMMatrixTranslation(left + fillHalfWidth, y + 0.012f, position.z));
        enemy.healthBarFill->SetWorldMatrix(fillWorld);
    }
}

void TowerDefenseScene::UpdateTowers(float timeElapsed)
{
    for (auto& tower : m_towers)
    {
        tower.boostTimer = max(0.0f, tower.boostTimer - timeElapsed);
        tower.cooldown = max(0.0f, tower.cooldown - timeElapsed);

        TowerDefenseEnemy* target = FindEnemyByObject(tower.target.lock());
        if (!target || !IsEnemyInTowerRange(tower, *target))
        {
            tower.target.reset();
            target = AcquireTowerTarget(tower);
            if (target)
            {
                tower.target = target->object;
            }
        }

        UpdateTowerAimVisual(tower, target, timeElapsed);

        if (!target || tower.cooldown > 0.0f) continue;

        const float targetScale = max(1.0f, target->visualScale);
        SpawnProjectile(tower, target->object);
        SpawnScopeMarker(target->object, min(0.34f, tower.fireInterval * 0.72f), 0.42f * targetScale);
        const float fireRateMultiplier = tower.boostTimer > 0.0f ? tower.boostFireRateMultiplier : 1.0f;
        tower.cooldown = tower.fireInterval / max(0.1f, fireRateMultiplier);
    }
}

void TowerDefenseScene::UpdateGenerators(float timeElapsed)
{
    for (auto& generator : m_generators)
    {
        if (!generator.object) continue;

        generator.timer -= timeElapsed;
        if (generator.timer > 0.0f) continue;

        AddGold(ScaleReward(generator.amount));
        XMFLOAT3 position = generator.object->GetPosition();
        SpawnCoinDropEffect(position, generator.amount);
        generator.timer = generator.interval;
    }
}

void TowerDefenseScene::UpdateProjectiles(float timeElapsed)
{
    for (auto& projectile : m_projectiles)
    {
        projectile.elapsed += timeElapsed;

        auto target = projectile.target.lock();
        if (!target || !projectile.object) continue;

        XMFLOAT3 end = target->GetPosition();
        end.y += 0.28f;
        const float t = clamp(projectile.elapsed / max(projectile.duration, 0.001f), 0.0f, 1.0f);
        XMFLOAT3 position = LerpPoint(projectile.start, end, t);
        if (projectile.arcHeight > 0.0f)
        {
            position.y += sinf(t * XM_PI) * projectile.arcHeight;
        }
        projectile.object->SetPosition(position);
    }

    for (auto it = m_projectiles.begin(); it != m_projectiles.end();)
    {
        if (it->elapsed >= it->duration || it->target.expired())
        {
            if (auto targetObject = it->target.lock())
            {
                if (auto enemy = FindEnemyByObject(targetObject))
                {
                    const XMFLOAT3 hitPosition = targetObject->GetPosition();
                    const float hitScale = max(0.90f, enemy->visualScale);
                    const float damageMultiplier =
                        (it->type == TowerDefenseTowerType::Flak && enemy->isFlying) ? 1.85f : 1.0f;
                    const float damage = it->damage * damageMultiplier;
                    enemy->health -= damage;
                    SpawnHitMarker(hitPosition, it->type, hitScale);
                    SpawnDamagePopup(hitPosition, damage, it->type, hitScale);

                    if (it->slowDuration > 0.0f)
                    {
                        enemy->slowTimer = max(enemy->slowTimer, it->slowDuration);
                        enemy->slowMultiplier = min(enemy->slowMultiplier, it->slowMultiplier);
                    }

                    if (it->splashRadius > 0.0f)
                    {
                        const float splashRadiusSq = it->splashRadius * it->splashRadius;
                        for (auto& splashEnemy : m_enemies)
                        {
                            if (!splashEnemy.object || splashEnemy.object == targetObject || splashEnemy.health <= 0.0f) continue;
                            if (it->type == TowerDefenseTowerType::Flak && !splashEnemy.isFlying) continue;
                            if (DistanceSqXZ(splashEnemy.object->GetPosition(), hitPosition) > splashRadiusSq) continue;

                            float splashDamage = it->damage * 0.58f;
                            if (it->type == TowerDefenseTowerType::Flak && splashEnemy.isFlying) splashDamage *= 1.45f;
                            splashEnemy.health -= splashDamage;
                            SpawnHitMarker(splashEnemy.object->GetPosition(), it->type, 0.80f);
                            SpawnDamagePopup(splashEnemy.object->GetPosition(), splashDamage, it->type, 0.80f);
                        }
                    }
                }
            }

            RemoveRenderObject(it->object);
            it = m_projectiles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void TowerDefenseScene::UpdateHitMarkers(float timeElapsed)
{
    for (auto& marker : m_hitMarkers)
    {
        marker.age += timeElapsed;
        marker.lifetime -= timeElapsed;
        marker.velocity.y -= marker.gravity * timeElapsed;
        if (marker.object)
        {
            XMFLOAT3 position = marker.object->GetPosition();
            position = Utiles::Vector3::Add(position, Utiles::Vector3::Mul(marker.velocity, timeElapsed));
            marker.object->SetPosition(position);
        }
    }

    for (auto it = m_hitMarkers.begin(); it != m_hitMarkers.end();)
    {
        if (it->lifetime <= 0.0f)
        {
            RemoveRenderObject(it->object);
            it = m_hitMarkers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void TowerDefenseScene::UpdateDamagePopups(float timeElapsed)
{
    for (auto& popup : m_damagePopups)
    {
        popup.lifetime -= timeElapsed;
        popup.position = Utiles::Vector3::Add(popup.position, Utiles::Vector3::Mul(popup.velocity, timeElapsed));
        popup.velocity.y += 0.35f * timeElapsed;
    }

    erase_if(m_damagePopups, [](const TowerDefenseDamagePopup& popup)
        {
            return popup.lifetime <= 0.0f;
        });
}

void TowerDefenseScene::UpdateRollingBoulders(float timeElapsed)
{
    for (auto& boulder : m_rollingBoulders)
    {
        boulder.lifetime -= timeElapsed;
        if (!boulder.object || boulder.routeIndex >= m_enemyPaths.size()) continue;

        const auto& path = m_enemyPaths[boulder.routeIndex];
        if (path.empty() || boulder.waypointIndex >= path.size()) continue;

        XMFLOAT3 target = path[boulder.waypointIndex];
        target.y = TerrainHeightAtWorldXZ(target.x, target.z) + boulder.radius;
        XMFLOAT3 delta = Utiles::Vector3::Sub(target, boulder.position);
        delta.y = 0.0f;
        const float distanceSq = Utiles::Vector3::Dot(delta, delta);
        const float maxMove = boulder.speed * timeElapsed;
        if (distanceSq <= maxMove * maxMove)
        {
            boulder.position = target;
            ++boulder.waypointIndex;
        }
        else if (distanceSq > Utiles::Physics::Epsilon)
        {
            const XMFLOAT3 direction = Normalize(delta);
            boulder.direction = direction;
            boulder.position = Utiles::Vector3::Add(boulder.position, Utiles::Vector3::Mul(direction, maxMove));
            boulder.position.y = TerrainHeightAtWorldXZ(boulder.position.x, boulder.position.z) + boulder.radius;
            boulder.rollAngle += maxMove / max(0.1f, boulder.radius);
        }

        for (auto& enemy : m_enemies)
        {
            if (!enemy.object || enemy.health <= 0.0f || enemy.isFlying) continue;
            const bool alreadyHit = any_of(boulder.hitEnemies.begin(), boulder.hitEnemies.end(),
                [&enemy](const weak_ptr<GameObject>& hit)
                {
                    return hit.lock() == enemy.object;
                });
            if (alreadyHit) continue;

            const float hitRadius = boulder.radius + max(0.42f, enemy.visualScale * 0.42f);
            if (DistanceSqXZ(boulder.position, enemy.object->GetPosition()) > hitRadius * hitRadius) continue;

            enemy.health -= boulder.damage;
            boulder.hitEnemies.push_back(enemy.object);
            const XMFLOAT3 hitPosition = enemy.object->GetPosition();
            SpawnHitMarker(hitPosition, TowerDefenseTowerType::Splash, max(1.0f, enemy.visualScale));
            SpawnDamagePopup(hitPosition, boulder.damage, TowerDefenseTowerType::Splash, max(1.0f, enemy.visualScale));
            for (int i = 0; i < 8; ++i)
            {
                const float angle = XM_2PI * static_cast<float>(i) / 8.0f;
                SpawnParticle(
                    XMFLOAT3{ hitPosition.x, hitPosition.y + 0.18f, hitPosition.z },
                    XMFLOAT3{ cosf(angle) * 1.35f, Utiles::Random::GetFloat(0.22f, 0.72f), sinf(angle) * 1.35f },
                    XMFLOAT3{ 0.10f, 0.055f, 0.10f },
                    m_boulderMaterial ? m_boulderMaterial : m_hitMaterial,
                    0.34f,
                    1.25f);
            }
        }

        const XMVECTOR rollAxis = XMVector3Normalize(XMVectorSet(boulder.direction.z, 0.0f, -boulder.direction.x, 0.0f));
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world,
            XMMatrixRotationAxis(rollAxis, boulder.rollAngle) *
            XMMatrixScaling(boulder.radius, boulder.radius, boulder.radius) *
            XMMatrixTranslation(boulder.position.x, boulder.position.y, boulder.position.z));
        boulder.object->SetWorldMatrix(world);
    }

    for (auto it = m_rollingBoulders.begin(); it != m_rollingBoulders.end();)
    {
        const bool finished = it->lifetime <= 0.0f ||
            it->routeIndex >= m_enemyPaths.size() ||
            it->waypointIndex >= m_enemyPaths[it->routeIndex].size();
        if (finished)
        {
            RemoveRenderObject(it->object);
            it = m_rollingBoulders.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void TowerDefenseScene::UpdateScopeMarkers(float timeElapsed)
{
    for (auto& marker : m_scopeMarkers)
    {
        marker.lifetime -= timeElapsed;
        PositionScopeMarker(marker);
    }

    for (auto it = m_scopeMarkers.begin(); it != m_scopeMarkers.end();)
    {
        if (it->lifetime <= 0.0f || it->target.expired())
        {
            for (const auto& object : it->objects)
            {
                RemoveRenderObject(object);
            }
            it = m_scopeMarkers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void TowerDefenseScene::SpawnEnemy(float healthMultiplier,
    float speedMultiplier,
    float visualScale,
    float laneOffset,
    bool isBoss,
    bool isFlying,
    bool splitsOnDeath,
    TowerDefenseEnemyVariant variant)
{
    if (m_enemyPaths.empty()) return;

    const size_t routeIndex = static_cast<size_t>(m_spawnedInWave) % m_enemyPaths.size();
    const auto& route = m_enemyPaths[routeIndex];
    if (route.empty()) return;

    visualScale = max(0.45f, visualScale);
    if (isBoss) variant = TowerDefenseEnemyVariant::Boss;
    else if (isFlying && splitsOnDeath) variant = TowerDefenseEnemyVariant::FlyingSplitter;
    else if (isFlying) variant = TowerDefenseEnemyVariant::FlyingScout;

    float baseHealth = 42.0f + static_cast<float>(m_wave - 1) * 10.0f;
    switch (variant)
    {
    case TowerDefenseEnemyVariant::Boss:
        baseHealth = 320.0f + static_cast<float>(m_wave) * 95.0f;
        break;
    case TowerDefenseEnemyVariant::Runner:
        baseHealth = 30.0f + static_cast<float>(m_wave - 1) * 6.5f;
        break;
    case TowerDefenseEnemyVariant::Brute:
        baseHealth = 82.0f + static_cast<float>(m_wave - 1) * 15.0f;
        break;
    case TowerDefenseEnemyVariant::Armored:
        baseHealth = 108.0f + static_cast<float>(m_wave - 1) * 18.0f;
        break;
    case TowerDefenseEnemyVariant::FlyingScout:
        baseHealth = 38.0f + static_cast<float>(m_wave - 1) * 8.0f;
        break;
    case TowerDefenseEnemyVariant::FlyingSplitter:
        baseHealth = 46.0f + static_cast<float>(m_wave - 1) * 9.5f;
        break;
    default:
        break;
    }
    const float health = baseHealth * healthMultiplier * GetDifficultyHealthMultiplier();
    XMFLOAT3 spawn = route.front();
    if (route.size() > 1 && fabsf(laneOffset) > 0.001f)
    {
        XMFLOAT3 segment = Utiles::Vector3::Sub(route[1], route[0]);
        XMFLOAT3 side = Normalize(XMFLOAT3{ -segment.z, 0.0f, segment.x });
        spawn.x += side.x * laneOffset;
        spawn.z += side.z * laneOffset;
    }
    float baseSpeed = 1.45f + static_cast<float>(m_wave - 1) * 0.08f;
    switch (variant)
    {
    case TowerDefenseEnemyVariant::Boss:
        baseSpeed = 0.72f + static_cast<float>(m_wave - 1) * 0.035f;
        break;
    case TowerDefenseEnemyVariant::Runner:
        baseSpeed = 2.18f + static_cast<float>(m_wave - 1) * 0.10f;
        break;
    case TowerDefenseEnemyVariant::Brute:
        baseSpeed = 1.08f + static_cast<float>(m_wave - 1) * 0.052f;
        break;
    case TowerDefenseEnemyVariant::Armored:
        baseSpeed = 0.92f + static_cast<float>(m_wave - 1) * 0.045f;
        break;
    case TowerDefenseEnemyVariant::FlyingScout:
        baseSpeed = 1.95f + static_cast<float>(m_wave - 1) * 0.095f;
        break;
    case TowerDefenseEnemyVariant::FlyingSplitter:
        baseSpeed = 1.72f + static_cast<float>(m_wave - 1) * 0.080f;
        break;
    default:
        break;
    }

    const float airOffset = isFlying ? 3.45f + visualScale * 0.70f : 0.0f;
    spawn.y = TerrainHeightAtWorldXZ(spawn.x, spawn.z) + EnemyHalfHeight * visualScale + airOffset;

    const string enemyName =
        variant == TowerDefenseEnemyVariant::Boss ? "Boss Enemy" :
        variant == TowerDefenseEnemyVariant::Runner ? "Runner Enemy" :
        variant == TowerDefenseEnemyVariant::Brute ? "Brute Enemy" :
        variant == TowerDefenseEnemyVariant::Armored ? "Armored Enemy" :
        variant == TowerDefenseEnemyVariant::FlyingSplitter ? "Flying Splitter" :
        variant == TowerDefenseEnemyVariant::FlyingScout ? "Flying Scout" :
        "Enemy";

    SpawnEnemyAt(
        enemyName,
        spawn,
        routeIndex,
        1,
        health,
        baseSpeed * speedMultiplier * GetDifficultySpeedMultiplier(),
        visualScale,
        isBoss,
        isFlying,
        splitsOnDeath,
        splitsOnDeath ? 4 + min(3, m_wave / 2) : 0,
        variant);

    if (isBoss)
    {
        m_cameraShakeDuration = 1.15f;
        m_cameraShakeTimer = m_cameraShakeDuration;
        m_cameraShakeIntensity = 0.58f;
        m_bossIntroTimer = 2.35f;

        for (int i = 0; i < 26; ++i)
        {
            const float angle = XM_2PI * static_cast<float>(i) / 26.0f;
            const float speed = Utiles::Random::GetFloat(1.35f, 3.15f);
            SpawnParticle(
                XMFLOAT3{ spawn.x, spawn.y + 0.20f, spawn.z },
                XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.65f, 1.80f), sinf(angle) * speed },
                XMFLOAT3{ 0.22f, 0.14f, 0.22f },
                i % 2 == 0 ? m_bossMaterial : m_deathEffectMaterial,
                0.82f,
                1.45f);
        }
    }
}

void TowerDefenseScene::SpawnEnemyAt(const string& name,
    const XMFLOAT3& position,
    size_t routeIndex,
    size_t waypointIndex,
    float health,
    float speed,
    float visualScale,
    bool isBoss,
    bool isFlying,
    bool splitsOnDeath,
    int splitCount,
    TowerDefenseEnemyVariant variant)
{
    visualScale = max(0.35f, visualScale);
    XMFLOAT3 spawn = position;
    const float groundOffset = EnemyHalfHeight * visualScale;
    const float airOffset = isFlying ? 3.45f + visualScale * 0.70f : 0.0f;
    const float desiredOffset = groundOffset + airOffset;
    spawn.y = max(spawn.y, TerrainHeightAtWorldXZ(spawn.x, spawn.z) + desiredOffset);

    const shared_ptr<Material>& material =
        variant == TowerDefenseEnemyVariant::Boss ? m_bossMaterial :
        variant == TowerDefenseEnemyVariant::Runner ? m_runnerEnemyMaterial :
        variant == TowerDefenseEnemyVariant::Armored ? m_armoredEnemyMaterial :
        variant == TowerDefenseEnemyVariant::Brute ? m_armoredEnemyMaterial :
        variant == TowerDefenseEnemyVariant::FlyingSplitter ? m_splitterEnemyMaterial :
        variant == TowerDefenseEnemyVariant::FlyingScout ? m_flyingEnemyMaterial :
        isBoss ? m_bossMaterial :
        isFlying ? m_flyingEnemyMaterial :
        m_enemyMaterial;
    auto enemyObject = CreateCapsuleObject(name,
        spawn,
        material,
        visualScale);
    EnableShadowCasting(enemyObject);
    enemyObject->SetCollider(make_shared<CapsuleCollider>(
        EnemyCapsuleRadius * visualScale,
        EnemyCapsuleBodyHeight * visualScale));

    auto rigidbody = make_shared<Rigidbody>();
    rigidbody->SetUseGravity(false);
    rigidbody->SetDrag(0.0f);
    rigidbody->SetGroundFriction(0.0f);
    rigidbody->SetMass(isBoss ? 2.5f : 1.0f);
    enemyObject->SetRigidbody(rigidbody);

    if (m_collisionManager) m_collisionManager->AddCollider(enemyObject->GetCollider());
    if (m_physicsManager) m_physicsManager->AddRigidbody(enemyObject->GetRigidbody());

    auto healthBack = CreateCubeObject("Enemy Health Back",
        XMFLOAT3{ spawn.x, spawn.y + 0.52f * visualScale, spawn.z },
        XMFLOAT3{ 0.28f * visualScale, 0.025f, 0.025f },
        m_healthBarBackMaterial);
    auto healthFill = CreateCubeObject("Enemy Health Fill",
        XMFLOAT3{ spawn.x, spawn.y + 0.54f * visualScale, spawn.z },
        XMFLOAT3{ 0.28f * visualScale, 0.028f, 0.028f },
        m_healthBarFillMaterial);

    m_objects.push_back(enemyObject);
    m_objects.push_back(healthBack);
    m_objects.push_back(healthFill);

    m_enemies.push_back(TowerDefenseEnemy{
        enemyObject,
        healthBack,
        healthFill,
        waypointIndex,
        routeIndex,
        health,
        health,
        speed,
        desiredOffset,
        0.0f,
        1.0f,
        visualScale,
        isBoss,
        isFlying,
        splitsOnDeath,
        max(0, splitCount),
        variant
    });
}

void TowerDefenseScene::SpawnSplitChildren(const TowerDefenseEnemy& parent, const XMFLOAT3& position)
{
    if (!parent.splitsOnDeath || parent.splitCount <= 0) return;
    const size_t routeIndex = parent.routeIndex;
    const size_t waypointIndex = parent.waypointIndex;
    const float childHealth = max(18.0f, parent.maxHealth * 0.16f);
    const float childSpeed = max(1.55f, parent.speed * 1.12f);

    for (int i = 0; i < parent.splitCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(parent.splitCount);
        const float radius = Utiles::Random::GetFloat(0.20f, 0.62f);
        XMFLOAT3 childPosition{
            position.x + cosf(angle) * radius,
            TerrainHeightAtWorldXZ(position.x, position.z) + EnemyHalfHeight * 0.58f,
            position.z + sinf(angle) * radius
        };
        childPosition.y = TerrainHeightAtWorldXZ(childPosition.x, childPosition.z) + EnemyHalfHeight * 0.58f;

        SpawnEnemyAt("Split Enemy",
            childPosition,
            routeIndex,
            waypointIndex,
            childHealth,
            childSpeed * Utiles::Random::GetFloat(0.92f, 1.12f),
            0.58f,
            false,
            false,
            false,
            0,
            TowerDefenseEnemyVariant::Runner);
    }
}

void TowerDefenseScene::SpawnHitMarker(const XMFLOAT3& position,
    TowerDefenseTowerType type,
    float scale)
{
    const int typeIndex = TowerTypeIndex(type);
    const shared_ptr<Material>& material = m_hitMaterials[typeIndex] ? m_hitMaterials[typeIndex] : m_hitMaterial;
    int particleCount = 4;
    float minSpeed = 0.45f;
    float maxSpeed = 0.95f;
    float lifetime = 0.20f;
    float gravity = 1.8f;
    XMFLOAT3 particleScale{ 0.07f, 0.07f, 0.07f };

    switch (type)
    {
    case TowerDefenseTowerType::Rapid:
        particleCount = 3;
        minSpeed = 0.80f;
        maxSpeed = 1.45f;
        lifetime = 0.16f;
        gravity = 1.2f;
        particleScale = XMFLOAT3{ 0.055f, 0.055f, 0.12f };
        break;
    case TowerDefenseTowerType::Splash:
        particleCount = 10;
        minSpeed = 0.95f;
        maxSpeed = 2.20f;
        lifetime = 0.34f;
        gravity = 2.4f;
        particleScale = XMFLOAT3{ 0.12f, 0.08f, 0.12f };
        break;
    case TowerDefenseTowerType::Slow:
        particleCount = 6;
        minSpeed = 0.25f;
        maxSpeed = 0.80f;
        lifetime = 0.42f;
        gravity = 0.55f;
        particleScale = XMFLOAT3{ 0.08f, 0.035f, 0.08f };
        break;
    case TowerDefenseTowerType::Mortar:
        particleCount = 16;
        minSpeed = 1.20f;
        maxSpeed = 2.85f;
        lifetime = 0.42f;
        gravity = 2.9f;
        particleScale = XMFLOAT3{ 0.15f, 0.11f, 0.15f };
        break;
    case TowerDefenseTowerType::Flak:
        particleCount = 8;
        minSpeed = 0.85f;
        maxSpeed = 1.80f;
        lifetime = 0.24f;
        gravity = 0.85f;
        particleScale = XMFLOAT3{ 0.07f, 0.07f, 0.14f };
        break;
    default:
        break;
    }

    scale = max(0.65f, scale);
    const int ringCount = type == TowerDefenseTowerType::Mortar ? 12 :
        type == TowerDefenseTowerType::Splash ? 10 : 8;
    const float ringSpeed = (type == TowerDefenseTowerType::Mortar ? 2.55f :
        type == TowerDefenseTowerType::Splash ? 2.10f : 1.45f) * scale;
    const XMFLOAT3 ringScale{
        (type == TowerDefenseTowerType::Mortar ? 0.22f : 0.16f) * scale,
        0.018f,
        0.035f * scale
    };
    for (int i = 0; i < ringCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(ringCount);
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.11f, position.z },
            XMFLOAT3{ cosf(angle) * ringSpeed, 0.08f, sinf(angle) * ringSpeed },
            ringScale,
            material,
            0.22f,
            0.12f);
    }

    for (int i = 0; i < particleCount; ++i)
    {
        const float angle = Utiles::Random::GetFloat(0.0f, XM_2PI);
        const float speed = Utiles::Random::GetFloat(minSpeed, maxSpeed) * scale;
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.30f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.18f, 0.78f) * scale, sinf(angle) * speed },
            XMFLOAT3{ particleScale.x * scale, particleScale.y * scale, particleScale.z * scale },
            material,
            lifetime,
            gravity);
    }
}

void TowerDefenseScene::SpawnDamagePopup(const XMFLOAT3& position,
    float damage,
    TowerDefenseTowerType type,
    float scale)
{
    if (damage <= 0.0f) return;

    WCHAR text[24]{};
    swprintf_s(text, L"%.0f", damage);
    m_damagePopups.push_back(TowerDefenseDamagePopup{
        XMFLOAT3{ position.x, position.y + 0.62f * max(0.85f, scale), position.z },
        XMFLOAT3{
            Utiles::Random::GetFloat(-0.16f, 0.16f),
            1.05f + Utiles::Random::GetFloat(0.0f, 0.28f),
            Utiles::Random::GetFloat(-0.16f, 0.16f)
        },
        text,
        0.72f,
        0.72f,
        0.022f + min(0.012f, damage * 0.00055f) + max(0.0f, scale - 1.0f) * 0.004f,
        type
        });
}

void TowerDefenseScene::SpawnDeathEffect(const XMFLOAT3& position, float scale)
{
    const int particleCount = 8 + static_cast<int>(scale * 3.0f);
    for (int i = 0; i < particleCount; ++i)
    {
        const float angle = Utiles::Random::GetFloat(0.0f, XM_2PI);
        const float speed = Utiles::Random::GetFloat(0.85f, 1.90f) * max(0.75f, scale);
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.25f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.45f, 1.35f) * scale, sinf(angle) * speed },
            XMFLOAT3{ 0.08f * scale, 0.08f * scale, 0.08f * scale },
            m_deathEffectMaterial,
            0.46f,
            2.6f);
    }
}

void TowerDefenseScene::SpawnCoinDropEffect(const XMFLOAT3& position, int amount)
{
    const int coinCount = clamp(amount + 1, 2, 5);
    for (int i = 0; i < coinCount; ++i)
    {
        const float angle = Utiles::Random::GetFloat(0.0f, XM_2PI);
        const float speed = Utiles::Random::GetFloat(0.30f, 0.85f);
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.45f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(1.00f, 1.65f), sinf(angle) * speed },
            XMFLOAT3{ 0.10f, 0.035f, 0.10f },
            m_coinMaterial,
            0.86f,
            2.0f);
    }
}

void TowerDefenseScene::SpawnMergeEffect(const XMFLOAT3& position, TowerDefenseTowerType type, int tier)
{
    const int typeIndex = TowerTypeIndex(type);
    const shared_ptr<Material>& material = m_hitMaterials[typeIndex] ? m_hitMaterials[typeIndex] : m_hitMaterial;
    const float tierScale = 0.85f + static_cast<float>(clamp(tier, 1, MaxTowerTier)) * 0.26f;
    const int particleCount = 10 + clamp(tier, 1, MaxTowerTier) * 4;

    for (int i = 0; i < particleCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(particleCount);
        const float speed = Utiles::Random::GetFloat(0.55f, 1.35f) * tierScale;
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.34f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.55f, 1.10f) * tierScale, sinf(angle) * speed },
            XMFLOAT3{ 0.08f * tierScale, 0.08f * tierScale, 0.08f * tierScale },
            material,
            0.44f,
            1.65f);
    }
}

void TowerDefenseScene::SpawnMeteorStrike(const XMFLOAT3& position, int tier)
{
    tier = clamp(tier, 1, MaxTowerTier);
    const float radius = MeteorRadius(tier);
    const float damage = MeteorDamage(tier, m_wave);
    const float radiusSq = radius * radius;

    for (auto& enemy : m_enemies)
    {
        if (!enemy.object || enemy.health <= 0.0f) continue;

        const XMFLOAT3 enemyPosition = enemy.object->GetPosition();
        const float distanceSq = DistanceSqXZ(position, enemyPosition);
        if (distanceSq > radiusSq) continue;

        const float distanceRatio = sqrtf(max(0.0f, distanceSq)) / max(radius, 0.001f);
        const float falloff = 1.0f - distanceRatio * 0.38f;
        const float airBonus = enemy.isFlying ? 0.78f : 1.0f;
        const float appliedDamage = damage * max(0.35f, falloff) * airBonus;
        enemy.health -= appliedDamage;
        SpawnHitMarker(enemyPosition, TowerDefenseTowerType::Splash, max(1.0f, enemy.visualScale));
        SpawnDamagePopup(enemyPosition, appliedDamage, TowerDefenseTowerType::Splash, max(1.0f, enemy.visualScale));
    }

    for (int i = 0; i < 5 + tier * 2; ++i)
    {
        const float angle = Utiles::Random::GetFloat(0.0f, XM_2PI);
        const float distance = Utiles::Random::GetFloat(0.0f, radius * 0.45f);
        XMFLOAT3 spawn{
            position.x + cosf(angle) * distance,
            position.y + 6.0f + static_cast<float>(tier) * 1.15f + static_cast<float>(i) * 0.18f,
            position.z + sinf(angle) * distance
        };
        SpawnParticle(
            spawn,
            XMFLOAT3{ cosf(angle) * 0.20f, -Utiles::Random::GetFloat(5.0f, 7.6f), sinf(angle) * 0.20f },
            XMFLOAT3{ 0.28f + tier * 0.08f, 0.28f + tier * 0.08f, 0.28f + tier * 0.08f },
            m_meteorMaterial,
            0.82f,
            0.0f);
    }

    const int burstCount = 18 + tier * 8;
    for (int i = 0; i < burstCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(burstCount);
        const float speed = Utiles::Random::GetFloat(1.25f, 3.60f) * (0.85f + tier * 0.18f);
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.26f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.62f, 2.35f), sinf(angle) * speed },
            XMFLOAT3{ 0.11f + tier * 0.035f, 0.09f + tier * 0.030f, 0.11f + tier * 0.035f },
            i % 3 == 0 ? m_deathEffectMaterial : m_meteorMaterial,
            0.55f + tier * 0.08f,
            2.25f);
    }
}

void TowerDefenseScene::SpawnFreezeField(const XMFLOAT3& position, int tier)
{
    tier = clamp(tier, 1, MaxTowerTier);
    const float radius = FreezeRadius(tier);
    const float radiusSq = radius * radius;
    const float duration = FreezeDuration(tier);
    const float slowMultiplier = max(0.25f, 0.50f - static_cast<float>(tier) * 0.06f);

    for (auto& enemy : m_enemies)
    {
        if (!enemy.object || enemy.health <= 0.0f) continue;
        if (DistanceSqXZ(position, enemy.object->GetPosition()) > radiusSq) continue;

        enemy.slowTimer = max(enemy.slowTimer, duration);
        enemy.slowMultiplier = min(enemy.slowMultiplier, slowMultiplier);
        SpawnHitMarker(enemy.object->GetPosition(), TowerDefenseTowerType::Slow, max(0.85f, enemy.visualScale));
    }

    const int particleCount = 18 + tier * 6;
    for (int i = 0; i < particleCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(particleCount);
        const float distance = Utiles::Random::GetFloat(radius * 0.18f, radius);
        SpawnParticle(
            XMFLOAT3{ position.x + cosf(angle) * distance, position.y + 0.16f, position.z + sinf(angle) * distance },
            XMFLOAT3{ cosf(angle) * Utiles::Random::GetFloat(0.12f, 0.50f), Utiles::Random::GetFloat(0.25f, 0.85f), sinf(angle) * Utiles::Random::GetFloat(0.12f, 0.50f) },
            XMFLOAT3{ 0.08f + tier * 0.018f, 0.025f, 0.08f + tier * 0.018f },
            m_freezeMaterial,
            0.78f,
            0.18f);
    }
}

void TowerDefenseScene::SpawnBoostEffect(const XMFLOAT3& position, int tier)
{
    tier = clamp(tier, 1, MaxTowerTier);
    const int particleCount = 12 + tier * 5;
    for (int i = 0; i < particleCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(particleCount);
        const float speed = Utiles::Random::GetFloat(0.45f, 1.20f);
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.42f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.75f, 1.80f), sinf(angle) * speed },
            XMFLOAT3{ 0.07f + tier * 0.018f, 0.07f + tier * 0.018f, 0.07f + tier * 0.018f },
            m_boostMaterial,
            0.64f,
            1.10f);
    }
}

void TowerDefenseScene::SpawnRollingBoulder(const XMFLOAT3& position, int tier)
{
    if (m_enemyPaths.empty()) return;

    tier = clamp(tier, 1, MaxTowerTier);
    size_t bestRoute = 0;
    size_t bestWaypoint = 1;
    XMFLOAT3 bestPoint = position;
    float bestDistanceSq = FLT_MAX;

    for (size_t routeIndex = 0; routeIndex < m_enemyPaths.size(); ++routeIndex)
    {
        const auto& path = m_enemyPaths[routeIndex];
        if (path.size() < 2) continue;

        for (size_t i = 1; i < path.size(); ++i)
        {
            XMVECTOR a = XMVectorSet(path[i - 1].x, 0.0f, path[i - 1].z, 0.0f);
            XMVECTOR b = XMVectorSet(path[i].x, 0.0f, path[i].z, 0.0f);
            XMVECTOR p = XMVectorSet(position.x, 0.0f, position.z, 0.0f);
            XMVECTOR ab = XMVectorSubtract(b, a);
            const float abLengthSq = XMVectorGetX(XMVector3Dot(ab, ab));
            if (abLengthSq <= Utiles::Physics::Epsilon) continue;

            float u = XMVectorGetX(XMVector3Dot(XMVectorSubtract(p, a), ab)) / abLengthSq;
            u = clamp(u, 0.0f, 1.0f);
            XMVECTOR closest = XMVectorAdd(a, XMVectorScale(ab, u));
            XMVECTOR delta = XMVectorSubtract(p, closest);
            const float distanceSq = XMVectorGetX(XMVector3Dot(delta, delta));
            if (distanceSq < bestDistanceSq)
            {
                XMFLOAT3 closestPoint{};
                XMStoreFloat3(&closestPoint, closest);
                bestDistanceSq = distanceSq;
                bestRoute = routeIndex;
                bestWaypoint = i;
                bestPoint = XMFLOAT3{ closestPoint.x, TerrainHeightAtWorldXZ(closestPoint.x, closestPoint.z), closestPoint.z };
            }
        }
    }

    const float radius = BoulderRadius(tier);
    bestPoint.y = TerrainHeightAtWorldXZ(bestPoint.x, bestPoint.z) + radius;

    auto object = make_shared<GameObject>();
    object->SetName("Rolling Boulder");
    object->SetMesh(m_boulderMesh ? m_boulderMesh : m_cube);
    object->SetMaterial(m_boulderMaterial ? m_boulderMaterial : m_tunnelStoneMaterial);
    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixScaling(radius, radius, radius) *
        XMMatrixTranslation(bestPoint.x, bestPoint.y, bestPoint.z));
    object->SetWorldMatrix(world);
    EnableShadowCasting(object);
    m_objects.push_back(object);

    TowerDefenseRollingBoulder boulder{};
    boulder.object = object;
    boulder.routeIndex = bestRoute;
    boulder.waypointIndex = bestWaypoint;
    boulder.position = bestPoint;
    boulder.radius = radius;
    boulder.speed = BoulderSpeed(tier);
    boulder.damage = BoulderDamage(tier, m_wave);
    boulder.lifetime = 7.5f + static_cast<float>(tier) * 1.4f;
    boulder.tier = tier;

    if (bestRoute < m_enemyPaths.size() && bestWaypoint < m_enemyPaths[bestRoute].size())
    {
        XMFLOAT3 target = m_enemyPaths[bestRoute][bestWaypoint];
        boulder.direction = Normalize(XMFLOAT3{ target.x - bestPoint.x, 0.0f, target.z - bestPoint.z });
    }

    for (int i = 0; i < 12; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) / 12.0f;
        SpawnParticle(
            XMFLOAT3{ bestPoint.x, bestPoint.y - radius * 0.55f, bestPoint.z },
            XMFLOAT3{ cosf(angle) * 1.15f, Utiles::Random::GetFloat(0.12f, 0.42f), sinf(angle) * 1.15f },
            XMFLOAT3{ 0.095f, 0.045f, 0.095f },
            m_boulderMaterial ? m_boulderMaterial : m_tunnelStoneMaterial,
            0.38f,
            0.85f);
    }

    m_rollingBoulders.push_back(std::move(boulder));
}

void TowerDefenseScene::SpawnParticle(const XMFLOAT3& position,
    const XMFLOAT3& velocity,
    const XMFLOAT3& scale,
    const shared_ptr<Material>& material,
    float lifetime,
    float gravity)
{
    auto markerObject = CreateCubeObject("Effect Particle",
        position,
        scale,
        material);
    m_objects.push_back(markerObject);
    m_hitMarkers.push_back(TowerDefenseHitMarker{
        markerObject,
        velocity,
        0.0f,
        max(0.01f, lifetime),
        gravity
    });
}

void TowerDefenseScene::SpawnScopeMarker(const shared_ptr<GameObject>& target, float duration, float radius)
{
    if (!target) return;

    TowerDefenseScopeMarker marker{};
    marker.target = target;
    marker.lifetime = max(duration, 0.10f);
    marker.radius = max(radius, 0.22f);

    const float barLength = marker.radius * 0.72f;
    const float thickness = 0.035f;
    marker.objects.push_back(CreateCubeObject("Scope Reticle Left",
        XMFLOAT3{ 0.0f, 0.0f, 0.0f },
        XMFLOAT3{ thickness, thickness, barLength },
        m_scopeMaterial));
    marker.objects.push_back(CreateCubeObject("Scope Reticle Right",
        XMFLOAT3{ 0.0f, 0.0f, 0.0f },
        XMFLOAT3{ thickness, thickness, barLength },
        m_scopeMaterial));
    marker.objects.push_back(CreateCubeObject("Scope Reticle Top",
        XMFLOAT3{ 0.0f, 0.0f, 0.0f },
        XMFLOAT3{ barLength, thickness, thickness },
        m_scopeMaterial));
    marker.objects.push_back(CreateCubeObject("Scope Reticle Bottom",
        XMFLOAT3{ 0.0f, 0.0f, 0.0f },
        XMFLOAT3{ barLength, thickness, thickness },
        m_scopeMaterial));
    marker.objects.push_back(CreateCubeObject("Scope Reticle Center",
        XMFLOAT3{ 0.0f, 0.0f, 0.0f },
        XMFLOAT3{ thickness * 1.2f, thickness * 1.2f, thickness * 1.2f },
        m_scopeMaterial));

    PositionScopeMarker(marker);
    m_objects.insert(m_objects.end(), marker.objects.begin(), marker.objects.end());
    m_scopeMarkers.push_back(std::move(marker));
}

void TowerDefenseScene::SpawnProjectile(const TowerDefenseTower& tower, const shared_ptr<GameObject>& target)
{
    if (!tower.object || !target) return;

    XMFLOAT3 start = GetTowerProjectileOrigin(tower);

    const float projectileSize = 0.18f + static_cast<float>(tower.tier) * 0.045f;
    XMFLOAT3 projectileScale{ projectileSize, projectileSize, projectileSize };
    float duration = 0.24f;
    float arcHeight = 0.0f;
    switch (tower.type)
    {
    case TowerDefenseTowerType::Rapid:
        projectileScale = XMFLOAT3{ projectileSize * 0.62f, projectileSize * 0.62f, projectileSize * 1.75f };
        duration = 0.15f;
        break;
    case TowerDefenseTowerType::Splash:
        projectileScale = XMFLOAT3{ projectileSize * 1.35f, projectileSize * 1.20f, projectileSize * 1.35f };
        duration = 0.29f;
        break;
    case TowerDefenseTowerType::Slow:
        projectileScale = XMFLOAT3{ projectileSize * 1.08f, projectileSize * 0.48f, projectileSize * 1.08f };
        duration = 0.28f;
        break;
    case TowerDefenseTowerType::Mortar:
        projectileScale = XMFLOAT3{ projectileSize * 2.15f, projectileSize * 2.15f, projectileSize * 2.15f };
        duration = 0.58f;
        arcHeight = 3.0f + static_cast<float>(tower.tier) * 0.55f;
        start.y += 0.35f;
        break;
    case TowerDefenseTowerType::Flak:
        projectileScale = XMFLOAT3{ projectileSize * 0.95f, projectileSize * 0.95f, projectileSize * 1.55f };
        duration = 0.20f;
        break;
    default:
        break;
    }

    const int typeIndex = TowerTypeIndex(tower.type);
    const shared_ptr<Material>& projectileMaterial =
        m_projectileMaterials[typeIndex] ? m_projectileMaterials[typeIndex] : m_projectileMaterial;
    auto projectileObject = CreateCubeObject("Tower Projectile",
        start,
        projectileScale,
        projectileMaterial);

    m_objects.push_back(projectileObject);
    m_projectiles.push_back(TowerDefenseProjectile{
        projectileObject,
        target,
        start,
        tower.type,
        tower.damage * (tower.boostTimer > 0.0f ? tower.boostDamageMultiplier : 1.0f),
        tower.splashRadius,
        tower.slowDuration,
        tower.slowMultiplier,
        0.0f,
        duration,
        arcHeight
    });
}

void TowerDefenseScene::UpdateDragPreview(HWND hWnd)
{
    if (!m_draggingTower || !m_dragGhost) return;

    XMFLOAT3 point{};
    if (!TryGetTerrainPoint(hWnd, point)) return;

    bool validCell = false;
    if (m_draggingPlacedTower)
    {
        validCell = CanMergeDraggedTowerAtPoint(point);
    }
    else
    {
        switch (m_dragOffer.kind)
        {
        case TowerDefenseOfferKind::Tower:
            validCell = CanPlaceTower(point) || CanMergeOfferAtPoint(point, m_dragOffer);
            break;
        case TowerDefenseOfferKind::Generator:
            validCell = CanPlaceTower(point);
            break;
        case TowerDefenseOfferKind::Boost:
            validCell = m_consumableCooldown <= 0.0f && !m_towers.empty();
            break;
        case TowerDefenseOfferKind::Boulder:
            validCell = m_consumableCooldown <= 0.0f && !m_enemyPaths.empty();
            break;
        default:
            validCell = m_consumableCooldown <= 0.0f;
            break;
        }
    }

    const bool isAreaConsumable =
        m_dragOffer.kind == TowerDefenseOfferKind::Meteor ||
        m_dragOffer.kind == TowerDefenseOfferKind::Freeze;
    XMFLOAT3 position{
        point.x,
        TerrainHeightAtWorldXZ(point.x, point.z) + (isAreaConsumable ? 0.10f : TowerHalfHeight),
        point.z
    };

    XMFLOAT4 ghostColor{ 1.0f, 0.12f, 0.10f, 0.30f };
    if (validCell)
    {
        if (m_draggingPlacedTower)
        {
            ghostColor = TowerColor(m_dragOffer.type, m_dragOffer.tier, 0.54f);
        }
        else switch (m_dragOffer.kind)
        {
        case TowerDefenseOfferKind::Meteor:
            ghostColor = XMFLOAT4{ 1.0f, 0.35f, 0.08f, 0.46f };
            break;
        case TowerDefenseOfferKind::Freeze:
            ghostColor = XMFLOAT4{ 0.20f, 0.78f, 1.0f, 0.42f };
            break;
        case TowerDefenseOfferKind::Boost:
            ghostColor = XMFLOAT4{ 0.35f, 1.0f, 0.18f, 0.50f };
            break;
        case TowerDefenseOfferKind::Boulder:
            ghostColor = XMFLOAT4{ 0.62f, 0.58f, 0.46f, 0.56f };
            break;
        case TowerDefenseOfferKind::Generator:
            ghostColor = XMFLOAT4{ 1.0f, 0.78f, 0.16f, 0.48f };
            break;
        default:
            ghostColor = TowerColor(m_dragOffer.type, m_dragOffer.tier, 0.42f);
            break;
        }
    }
    m_dragGhostMaterial->SetBaseColor(ghostColor);

    if (m_dragOffer.kind == TowerDefenseOfferKind::Tower &&
        (!m_dragGhostParts.empty() || m_towerModelMesh))
    {
        UpdateTowerDragGhostVisual(point);
        return;
    }

    XMFLOAT4X4 world{};
    XMFLOAT3 visualSize = TowerVisualSize(m_dragOffer.type, m_dragOffer.tier);
    switch (m_dragOffer.kind)
    {
    case TowerDefenseOfferKind::Meteor:
        visualSize = XMFLOAT3{ MeteorRadius(m_dragOffer.tier), 0.035f, MeteorRadius(m_dragOffer.tier) };
        break;
    case TowerDefenseOfferKind::Freeze:
        visualSize = XMFLOAT3{ FreezeRadius(m_dragOffer.tier), 0.035f, FreezeRadius(m_dragOffer.tier) };
        break;
    case TowerDefenseOfferKind::Boost:
        visualSize = XMFLOAT3{ 2.40f, 0.050f, 2.40f };
        break;
    case TowerDefenseOfferKind::Boulder:
    {
        const float radius = BoulderRadius(m_dragOffer.tier);
        visualSize = XMFLOAT3{ radius, radius, radius };
        position.y = TerrainHeightAtWorldXZ(point.x, point.z) + radius;
        break;
    }
    case TowerDefenseOfferKind::Generator:
    {
        const float tierScale = TowerVisualScale(m_dragOffer.tier);
        visualSize = XMFLOAT3{ 0.45f * tierScale, 0.34f * tierScale, 0.45f * tierScale };
        position.y = TerrainHeightAtWorldXZ(point.x, point.z) + TowerHalfHeight * 0.48f;
        break;
    }
    default:
        break;
    }
    XMStoreFloat4x4(&world, XMMatrixScaling(visualSize.x, visualSize.y, visualSize.z) *
        XMMatrixTranslation(position.x, position.y, position.z));
    m_dragGhost->SetWorldMatrix(world);
}

bool TowerDefenseScene::TryPlaceTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Tower) return false;
    if (!CanPlaceTower(terrainPoint) || m_gold < offer.cost) return false;

    const int tier = clamp(offer.tier, 1, MaxTowerTier);
    TowerDefenseTower tower{};
    tower.type = offer.type;
    tower.tier = tier;

    shared_ptr<GameObject> towerObject;
    if (!m_towerModelParts.empty())
    {
        towerObject = make_shared<GameObject>();
        towerObject->SetName("Tower");
        towerObject->SetWorldMatrix(BuildTowerModelRootMatrix(terrainPoint, offer.type, tier));

        shared_ptr<Mesh> colliderMesh = m_towerModelMesh;
        if (!colliderMesh && !m_towerModelParts.empty()) colliderMesh = m_towerModelParts.front().mesh;
        towerObject->SetCollider(make_shared<BoxCollider>(colliderMesh ? colliderMesh : m_cube));
        if (m_collisionManager) m_collisionManager->AddCollider(towerObject->GetCollider());
        m_objects.push_back(towerObject);

        tower.object = towerObject;
        tower.position = towerObject->GetPosition();
        tower.parts.reserve(m_towerModelParts.size());

        for (size_t i = 0; i < m_towerModelParts.size(); ++i)
        {
            const TowerDefenseTowerModelPart& asset = m_towerModelParts[i];
            if (!asset.mesh) continue;

            auto partObject = make_shared<GameObject>();
            partObject->SetName("Tower Part " + asset.name);
            partObject->SetMesh(asset.mesh);
            const int typeIndex = TowerTypeIndex(offer.type);
            if (typeIndex < static_cast<int>(asset.materials.size()) &&
                tier - 1 < static_cast<int>(asset.materials[typeIndex].size()))
            {
                partObject->SetMaterial(asset.materials[typeIndex][tier - 1]);
            }
            EnableShadowCasting(partObject);
            m_objects.push_back(partObject);

            tower.parts.push_back(TowerDefenseTowerPartInstance{
                partObject,
                i,
                asset.rotatesWithTarget
            });
        }

        ApplyTowerStats(tower);
        ApplyTowerModelPartTransforms(tower);
    }
    else
    {
        towerObject = CreateTowerObject("Tower", terrainPoint, offer.type, tier);
        EnableShadowCasting(towerObject);
        towerObject->SetCollider(make_shared<BoxCollider>(m_towerModelMesh ? m_towerModelMesh : m_cube));
        if (m_collisionManager) m_collisionManager->AddCollider(towerObject->GetCollider());
        m_objects.push_back(towerObject);

        tower.object = towerObject;
        tower.position = towerObject->GetPosition();
        ApplyTowerStats(tower);
    }

    m_towers.push_back(tower);
    m_selectedTower = towerObject;
    m_gold -= offer.cost;
    ++m_towersPlaced;
    m_highestTowerTier = max(m_highestTowerTier, tier);
    return true;
}

bool TowerDefenseScene::TryCastMeteor(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Meteor || m_mode != TowerDefenseMode::Playing) return false;
    if (!m_terrainCollider || m_gold < offer.cost) return false;
    if (m_consumableCooldown > 0.0f) return false;

    float terrainHeight = 0.0f;
    XMFLOAT3 terrainNormal{};
    if (!m_terrainCollider->GetHeightAtWorld(terrainPoint, terrainHeight, terrainNormal)) return false;

    XMFLOAT3 impact{
        terrainPoint.x,
        terrainHeight,
        terrainPoint.z
    };
    m_gold -= offer.cost;
    SpawnMeteorStrike(impact, offer.tier);
    m_consumableCooldown = m_consumableCooldownDuration;
    return true;
}

bool TowerDefenseScene::TryCastFreeze(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Freeze || m_mode != TowerDefenseMode::Playing) return false;
    if (!m_terrainCollider || m_gold < offer.cost) return false;
    if (m_consumableCooldown > 0.0f) return false;

    float terrainHeight = 0.0f;
    XMFLOAT3 terrainNormal{};
    if (!m_terrainCollider->GetHeightAtWorld(terrainPoint, terrainHeight, terrainNormal)) return false;

    XMFLOAT3 center{ terrainPoint.x, terrainHeight, terrainPoint.z };
    m_gold -= offer.cost;
    SpawnFreezeField(center, offer.tier);
    m_consumableCooldown = m_consumableCooldownDuration;
    return true;
}

bool TowerDefenseScene::TryCastBoulder(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Boulder || m_mode != TowerDefenseMode::Playing) return false;
    if (m_enemyPaths.empty() || m_gold < offer.cost) return false;
    if (m_consumableCooldown > 0.0f) return false;

    m_gold -= offer.cost;
    SpawnRollingBoulder(terrainPoint, offer.tier);
    m_consumableCooldown = m_consumableCooldownDuration;
    return true;
}

bool TowerDefenseScene::TryBoostTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Boost || m_mode != TowerDefenseMode::Playing) return false;
    if (m_gold < offer.cost || m_consumableCooldown > 0.0f) return false;

    TowerDefenseTower* tower = nullptr;
    float bestDistanceSq = 2.40f * 2.40f;
    for (auto& candidate : m_towers)
    {
        if (!candidate.object) continue;

        const float distanceSq = DistanceSqXZ(candidate.position, terrainPoint);
        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            tower = &candidate;
        }
    }

    if (!tower)
    {
        if (auto selected = m_selectedTower.lock())
        {
            const int index = FindTowerIndexByObject(selected);
            if (index >= 0 && index < static_cast<int>(m_towers.size()))
            {
                tower = &m_towers[index];
            }
        }
    }
    if (!tower || !tower->object) return false;

    const int tier = clamp(offer.tier, 1, MaxTowerTier);
    const float multiplier = BoostMultiplier(tier);
    tower->boostTimer = max(tower->boostTimer, BoostDuration(tier));
    tower->boostDamageMultiplier = max(tower->boostDamageMultiplier, multiplier);
    tower->boostFireRateMultiplier = max(tower->boostFireRateMultiplier, 1.15f + static_cast<float>(tier) * 0.18f);
    m_gold -= offer.cost;
    SpawnBoostEffect(tower->object->GetPosition(), tier);
    m_consumableCooldown = m_consumableCooldownDuration;
    return true;
}

bool TowerDefenseScene::TryPlaceGenerator(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Generator) return false;
    if (!CanPlaceTower(terrainPoint) || m_gold < offer.cost) return false;

    const int tier = clamp(offer.tier, 1, MaxTowerTier);
    const float tierScale = TowerVisualScale(tier);
    XMFLOAT3 center{
        terrainPoint.x,
        TerrainHeightAtWorldXZ(terrainPoint.x, terrainPoint.z) + TowerHalfHeight * 0.48f,
        terrainPoint.z
    };
    auto generatorObject = CreateCubeObject("Coin Generator",
        center,
        XMFLOAT3{ 0.45f * tierScale, 0.34f * tierScale, 0.45f * tierScale },
        m_generatorMaterial);
    EnableShadowCasting(generatorObject);
    generatorObject->SetCollider(make_shared<BoxCollider>(m_cube));
    if (m_collisionManager) m_collisionManager->AddCollider(generatorObject->GetCollider());

    m_objects.push_back(generatorObject);
    m_generators.push_back(TowerDefenseGenerator{
        generatorObject,
        center,
        tier,
        GeneratorInterval(tier),
        GeneratorInterval(tier),
        GeneratorAmount(tier)
    });
    m_gold -= offer.cost;
    SpawnCoinDropEffect(center, tier);
    return true;
}

bool TowerDefenseScene::TryMergeOfferWithTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Tower || m_gold < offer.cost) return false;

    TowerDefenseTower* target = FindTowerAtPoint(terrainPoint);
    if (!target || !target->object) return false;
    if (target->type != offer.type || target->tier != offer.tier || target->tier >= MaxTowerTier) return false;

    const XMFLOAT3 mergePosition = target->object->GetPosition();
    target->tier += 1;
    ApplyTowerStats(*target);
    UpdateTowerVisual(*target);
    target->cooldown = 0.0f;
    target->target.reset();

    m_gold -= offer.cost;
    m_selectedTower = target->object;
    ++m_towersMerged;
    m_highestTowerTier = max(m_highestTowerTier, target->tier);
    SpawnMergeEffect(mergePosition, target->type, target->tier);
    return true;
}

bool TowerDefenseScene::TryMergeDraggedTowerWith(const XMFLOAT3& terrainPoint)
{
    auto sourceObject = m_dragSourceTower.lock();
    if (!sourceObject) return false;

    const int sourceIndex = FindTowerIndexByObject(sourceObject);
    TowerDefenseTower* target = FindTowerAtPoint(terrainPoint);
    if (sourceIndex < 0 || !target || !target->object || target->object == sourceObject) return false;

    const int targetIndex = FindTowerIndexByObject(target->object);
    if (targetIndex < 0 || targetIndex == sourceIndex) return false;

    TowerDefenseTower& source = m_towers[sourceIndex];
    TowerDefenseTower& survivor = m_towers[targetIndex];
    if (source.type != survivor.type || source.tier != survivor.tier || survivor.tier >= MaxTowerTier) return false;

    const shared_ptr<GameObject> survivorObject = survivor.object;
    const XMFLOAT3 survivorPosition = survivorObject ? survivorObject->GetPosition() : survivor.position;

    survivor.tier += 1;
    ApplyTowerStats(survivor);
    UpdateTowerVisual(survivor);
    survivor.cooldown = 0.0f;
    survivor.target.reset();

    const int mergedTier = survivor.tier;
    SpawnMergeEffect(survivorPosition, survivor.type, survivor.tier);
    RemoveTowerInstanceObjects(source);
    m_towers.erase(m_towers.begin() + sourceIndex);
    m_selectedTower = survivorObject;
    ++m_towersMerged;
    m_highestTowerTier = max(m_highestTowerTier, mergedTier);
    return true;
}

void TowerDefenseScene::ApplyTowerStats(TowerDefenseTower& tower)
{
    tower.tier = clamp(tower.tier, 1, MaxTowerTier);
    const TowerStats stats = BuildTowerStats(tower.type, tower.tier);
    tower.minRange = stats.minRange;
    tower.range = stats.range;
    tower.damage = stats.damage * GetTowerDamageMultiplier(tower.type);
    tower.fireInterval = stats.fireInterval;
    tower.splashRadius = stats.splashRadius;
    tower.slowDuration = stats.slowDuration;
    tower.slowMultiplier = stats.slowMultiplier;
    tower.targetsGround = stats.targetsGround;
    tower.targetsAir = stats.targetsAir;
}

void TowerDefenseScene::UpdateTowerVisual(TowerDefenseTower& tower)
{
    if (!tower.object) return;

    tower.tier = clamp(tower.tier, 1, MaxTowerTier);
    const int typeIndex = TowerTypeIndex(tower.type);
    if (!tower.parts.empty())
    {
        const XMFLOAT3 current = tower.object->GetPosition();
        tower.object->SetWorldMatrix(BuildTowerModelRootMatrix(current, tower.type, tower.tier));
        tower.position = tower.object->GetPosition();

        for (TowerDefenseTowerPartInstance& instance : tower.parts)
        {
            if (instance.assetIndex >= m_towerModelParts.size() || !instance.object) continue;

            const TowerDefenseTowerModelPart& asset = m_towerModelParts[instance.assetIndex];
            instance.object->SetMesh(asset.mesh);
            if (typeIndex < static_cast<int>(asset.materials.size()) &&
                tower.tier - 1 < static_cast<int>(asset.materials[typeIndex].size()))
            {
                instance.object->SetMaterial(asset.materials[typeIndex][tower.tier - 1]);
            }
            instance.rotatesWithTarget = asset.rotatesWithTarget;
        }

        ApplyTowerModelPartTransforms(tower);
        return;
    }

    if (m_towerModelMesh)
    {
        tower.object->SetMesh(m_towerModelMesh);
        tower.object->SetMaterial(m_towerModelMaterials[typeIndex][tower.tier - 1]
            ? m_towerModelMaterials[typeIndex][tower.tier - 1]
            : m_towerMaterials[typeIndex][tower.tier - 1]);

        const XMFLOAT3 current = tower.object->GetPosition();
        tower.object->SetWorldMatrix(BuildTowerModelRootMatrix(current, tower.type, tower.tier));
        tower.position = tower.object->GetPosition();
        return;
    }

    tower.object->SetMaterial(m_towerMaterials[typeIndex][tower.tier - 1]);

    const XMFLOAT3 visualSize = TowerVisualSize(tower.type, tower.tier);
    const XMFLOAT3 position = tower.object->GetPosition();
    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixScaling(visualSize.x, visualSize.y, visualSize.z) *
        XMMatrixTranslation(position.x, position.y, position.z));
    tower.object->SetWorldMatrix(world);
    tower.position = position;
}

void TowerDefenseScene::UpdateTowerAimVisual(TowerDefenseTower& tower,
    const TowerDefenseEnemy* target,
    float timeElapsed)
{
    if (!tower.object) return;

    if (target && target->object)
    {
        const XMFLOAT3 from = tower.object->GetPosition();
        const XMFLOAT3 to = target->object->GetPosition();
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        if (fabsf(dx) + fabsf(dz) > 0.001f)
        {
            const float desiredYaw = atan2f(dx, dz);
            tower.turretYaw = MoveAngleTowards(tower.turretYaw, desiredYaw, max(0.0f, timeElapsed) * 7.5f);
        }
    }

    if (!tower.parts.empty())
    {
        ApplyTowerModelPartTransforms(tower);
        return;
    }
}

void TowerDefenseScene::ApplyTowerModelPartTransforms(TowerDefenseTower& tower) const
{
    if (!tower.object || tower.parts.empty()) return;

    const XMFLOAT4X4 rootWorld = tower.object->GetWorldMatrix();
    const XMMATRIX rootMatrix = XMLoadFloat4x4(&rootWorld);
    for (TowerDefenseTowerPartInstance& instance : tower.parts)
    {
        if (instance.assetIndex >= m_towerModelParts.size() || !instance.object) continue;

        const TowerDefenseTowerModelPart& asset = m_towerModelParts[instance.assetIndex];
        const XMMATRIX localMatrix = XMLoadFloat4x4(&asset.localMatrix);
        const XMMATRIX aimMatrix = instance.rotatesWithTarget
            ? XMMatrixRotationY(tower.turretYaw)
            : XMMatrixIdentity();

        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, aimMatrix * localMatrix * rootMatrix);
        instance.object->SetWorldMatrix(world);
    }
}

void TowerDefenseScene::RemoveTowerInstanceObjects(TowerDefenseTower& tower)
{
    for (TowerDefenseTowerPartInstance& instance : tower.parts)
    {
        RemoveRenderObject(instance.object);
    }
    tower.parts.clear();
    RemoveSimulationObject(tower.object);
}

void TowerDefenseScene::MouseEvent(HWND hWnd, FLOAT timeElapsed)
{
    if (m_debugCameraEnabled)
    {
        Scene::MouseEvent(hWnd, timeElapsed);
        return;
    }

    if (m_rightMouseOrbiting && !m_topDownView) UpdateAngledMouseOrbit(hWnd);
    if (m_draggingTower) UpdateDragPreview(hWnd);
}

void TowerDefenseScene::MouseButtonEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    if (m_debugCameraEnabled) return;

    if (message == WM_RBUTTONDOWN)
    {
        POINT cursor{};
        float width = 0.0f;
        float height = 0.0f;
        if (GetClientCursor(hWnd, cursor, width, height))
        {
            m_rightMouseOrbiting = true;
            m_lastOrbitCursor = cursor;
        }
        return;
    }

    if (message == WM_RBUTTONUP)
    {
        m_rightMouseOrbiting = false;
        return;
    }

    (void)wParam;
    if (message == WM_LBUTTONDOWN)
    {
        HandleClick(hWnd);
        if (m_draggingTower) UpdateDragPreview(hWnd);
    }
    else if (message == WM_LBUTTONUP)
    {
        if (m_draggingTower) EndTowerDrag(hWnd);
    }
}

void TowerDefenseScene::HandleClick(HWND hWnd)
{
    if (m_mode == TowerDefenseMode::StartScreen)
    {
        TryHandleStartMenuClick(hWnd);
        return;
    }

    if (m_bossRewardPending)
    {
        int rewardChoice = 0;
        if (TryGetBossRewardChoiceFromCursor(hWnd, rewardChoice))
        {
            ApplyBossRewardChoice(rewardChoice);
        }
        return;
    }

    if (m_waveRewardPending)
    {
        int rewardChoice = 0;
        if (TryGetWaveRewardChoiceFromCursor(hWnd, rewardChoice))
        {
            ApplyWaveRewardChoice(rewardChoice);
        }
        return;
    }

    if (IsShopToggleUnderCursor(hWnd))
    {
        ToggleShopCollapsed();
        return;
    }

    int upgradeType = -1;
    if (TryGetTowerUpgradeChoiceFromCursor(hWnd, upgradeType))
    {
        TryUpgradeTowerDamage(upgradeType);
        return;
    }

    if (IsWaveToggleUnderCursor(hWnd))
    {
        ToggleWaveRunning();
        return;
    }

    int offerSlot = 0;
    if (TryGetShopSlotFromCursor(hWnd, offerSlot))
    {
        m_selectedTower.reset();
        BeginTowerDrag(offerSlot);
        return;
    }

    if (IsTowerInfoPanelUnderCursor(hWnd)) return;

    XMFLOAT3 terrainPoint{};
    if (TryGetTerrainPoint(hWnd, terrainPoint))
    {
        if (TowerDefenseTower* tower = FindTowerAtPoint(terrainPoint))
        {
            BeginPlacedTowerDrag(tower->object);
        }
        else
        {
            m_selectedTower.reset();
        }
    }
}

void TowerDefenseScene::BeginTowerDrag(int offerSlot)
{
    offerSlot = clamp(offerSlot, 1, 3);
    ClearDragGhost();
    m_dragOffer = m_shopOffers[offerSlot - 1];
    if (m_gold < m_dragOffer.cost) return;
    if (IsConsumableOffer(m_dragOffer.kind) && m_consumableCooldown > 0.0f) return;

    m_draggingTower = true;
    m_dragOfferSlot = offerSlot;
    m_dragTier = clamp(m_dragOffer.tier, 1, MaxTowerTier);
    XMFLOAT4 dragColor = TowerColor(m_dragOffer.type, m_dragOffer.tier, 0.38f);
    XMFLOAT3 visualSize = TowerVisualSize(m_dragOffer.type, m_dragTier);
    switch (m_dragOffer.kind)
    {
    case TowerDefenseOfferKind::Meteor:
        dragColor = XMFLOAT4{ 1.0f, 0.36f, 0.08f, 0.38f };
        visualSize = XMFLOAT3{ MeteorRadius(m_dragTier), 0.035f, MeteorRadius(m_dragTier) };
        break;
    case TowerDefenseOfferKind::Freeze:
        dragColor = XMFLOAT4{ 0.20f, 0.78f, 1.0f, 0.38f };
        visualSize = XMFLOAT3{ FreezeRadius(m_dragTier), 0.035f, FreezeRadius(m_dragTier) };
        break;
    case TowerDefenseOfferKind::Boost:
        dragColor = XMFLOAT4{ 0.35f, 1.0f, 0.18f, 0.42f };
        visualSize = XMFLOAT3{ 2.40f, 0.050f, 2.40f };
        break;
    case TowerDefenseOfferKind::Boulder:
    {
        const float radius = BoulderRadius(m_dragTier);
        dragColor = XMFLOAT4{ 0.56f, 0.52f, 0.42f, 0.48f };
        visualSize = XMFLOAT3{ radius, radius, radius };
        break;
    }
    case TowerDefenseOfferKind::Generator:
    {
        const float tierScale = TowerVisualScale(m_dragTier);
        dragColor = XMFLOAT4{ 1.0f, 0.78f, 0.16f, 0.42f };
        visualSize = XMFLOAT3{ 0.45f * tierScale, 0.34f * tierScale, 0.45f * tierScale };
        break;
    }
    default:
        break;
    }
    m_dragGhostMaterial->SetBaseColor(dragColor);
    if (m_dragOffer.kind == TowerDefenseOfferKind::Tower &&
        BuildTowerDragGhostVisual(m_dragOffer))
    {
        return;
    }

    m_dragGhost = CreateCubeObject("Tower Drag Preview",
        XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
        visualSize,
        m_dragGhostMaterial);
    if (m_dragOffer.kind == TowerDefenseOfferKind::Boulder && m_boulderMesh)
    {
        m_dragGhost->SetMesh(m_boulderMesh);
    }
    m_objects.push_back(m_dragGhost);
}

void TowerDefenseScene::BeginPlacedTowerDrag(const shared_ptr<GameObject>& towerObject)
{
    ClearDragGhost();

    const int towerIndex = FindTowerIndexByObject(towerObject);
    if (towerIndex < 0) return;

    const TowerDefenseTower& tower = m_towers[towerIndex];
    m_dragOffer = TowerDefenseOffer{};
    m_dragOffer.kind = TowerDefenseOfferKind::Tower;
    m_dragOffer.type = tower.type;
    m_dragOffer.tier = tower.tier;
    m_dragOffer.cost = 0;
    m_dragTier = clamp(tower.tier, 1, MaxTowerTier);
    m_dragOfferSlot = 0;
    m_draggingTower = true;
    m_draggingPlacedTower = true;
    m_dragSourceTower = towerObject;
    m_selectedTower = towerObject;

    m_dragGhostMaterial->SetBaseColor(TowerColor(tower.type, tower.tier, 0.38f));
    if (BuildTowerDragGhostVisual(m_dragOffer))
    {
        return;
    }

    m_dragGhost = CreateCubeObject("Placed Tower Drag Preview",
        XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
        TowerVisualSize(tower.type, tower.tier),
        m_dragGhostMaterial);
    m_objects.push_back(m_dragGhost);
}

void TowerDefenseScene::EndTowerDrag(HWND hWnd)
{
    bool placed = false;
    auto sourceObject = m_dragSourceTower.lock();
    XMFLOAT3 point{};
    if (TryGetTerrainPoint(hWnd, point))
    {
        if (m_draggingPlacedTower)
        {
            placed = TryMergeDraggedTowerWith(point);
        }
        else
        {
            switch (m_dragOffer.kind)
            {
            case TowerDefenseOfferKind::Meteor:
                placed = TryCastMeteor(point, m_dragOffer);
                break;
            case TowerDefenseOfferKind::Freeze:
                placed = TryCastFreeze(point, m_dragOffer);
                break;
            case TowerDefenseOfferKind::Boost:
                placed = TryBoostTower(point, m_dragOffer);
                break;
            case TowerDefenseOfferKind::Boulder:
                placed = TryCastBoulder(point, m_dragOffer);
                break;
            case TowerDefenseOfferKind::Generator:
                placed = TryPlaceGenerator(point, m_dragOffer);
                break;
            default:
                placed = TryMergeOfferWithTower(point, m_dragOffer);
                if (!placed) placed = TryPlaceTower(point, m_dragOffer);
                break;
            }
        }
    }

    if (m_draggingPlacedTower && !placed && sourceObject)
    {
        m_selectedTower = sourceObject;
    }

    if (placed && m_dragOfferSlot >= 1 && m_dragOfferSlot <= 3)
    {
        m_shopOffers[m_dragOfferSlot - 1] = CreateRandomOffer();
    }

    ClearDragGhost();
}

bool TowerDefenseScene::BuildTowerDragGhostVisual(const TowerDefenseOffer& offer)
{
    if (offer.kind != TowerDefenseOfferKind::Tower) return false;

    XMFLOAT4X4 hiddenWorld{};
    XMStoreFloat4x4(&hiddenWorld, XMMatrixTranslation(0.0f, -1000.0f, 0.0f));

    m_dragGhostParts.clear();
    if (!m_towerModelParts.empty())
    {
        m_dragGhost = make_shared<GameObject>();
        m_dragGhost->SetName("Tower Drag Preview Root");
        m_dragGhost->SetWorldMatrix(hiddenWorld);

        for (size_t i = 0; i < m_towerModelParts.size(); ++i)
        {
            const TowerDefenseTowerModelPart& asset = m_towerModelParts[i];
            if (!asset.mesh) continue;

            auto partObject = make_shared<GameObject>();
            partObject->SetName("Tower Drag Preview " + asset.name);
            partObject->SetMesh(asset.mesh);
            partObject->SetMaterial(m_dragGhostMaterial);
            partObject->SetWorldMatrix(hiddenWorld);
            m_objects.push_back(partObject);

            m_dragGhostParts.push_back(TowerDefenseTowerPartInstance{
                partObject,
                i,
                false
            });
        }

        return !m_dragGhostParts.empty();
    }

    if (m_towerModelMesh)
    {
        m_dragGhost = make_shared<GameObject>();
        m_dragGhost->SetName("Tower Drag Preview Model");
        m_dragGhost->SetMesh(m_towerModelMesh);
        m_dragGhost->SetMaterial(m_dragGhostMaterial);
        m_dragGhost->SetWorldMatrix(hiddenWorld);
        m_objects.push_back(m_dragGhost);
        return true;
    }

    return false;
}

void TowerDefenseScene::UpdateTowerDragGhostVisual(const XMFLOAT3& terrainPoint)
{
    if (!m_dragGhost) return;

    const XMFLOAT4X4 rootWorld = BuildTowerModelRootMatrix(
        terrainPoint,
        m_dragOffer.type,
        m_dragOffer.tier);
    m_dragGhost->SetWorldMatrix(rootWorld);

    if (m_dragGhostParts.empty()) return;

    const XMMATRIX rootMatrix = XMLoadFloat4x4(&rootWorld);
    for (TowerDefenseTowerPartInstance& instance : m_dragGhostParts)
    {
        if (instance.assetIndex >= m_towerModelParts.size() || !instance.object) continue;

        const TowerDefenseTowerModelPart& asset = m_towerModelParts[instance.assetIndex];
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMLoadFloat4x4(&asset.localMatrix) * rootMatrix);
        instance.object->SetWorldMatrix(world);
    }
}

void TowerDefenseScene::ClearDragGhost()
{
    for (TowerDefenseTowerPartInstance& instance : m_dragGhostParts)
    {
        RemoveRenderObject(instance.object);
    }
    m_dragGhostParts.clear();

    if (m_dragGhost)
    {
        RemoveRenderObject(m_dragGhost);
        m_dragGhost.reset();
    }

    m_draggingTower = false;
    m_draggingPlacedTower = false;
    m_dragSourceTower.reset();
    m_dragOfferSlot = 0;
    m_dragTier = 1;
    m_dragOffer = TowerDefenseOffer{};
}

void TowerDefenseScene::EnableShadowCasting(const shared_ptr<GameObject>& object) const
{
    if (!object || object->GetComponent<ShadowCasterComponent>()) return;

    object->AddComponent<ShadowCasterComponent>(true);
}

void TowerDefenseScene::KeyboardEvent(FLOAT timeElapsed)
{
    if (m_debugCameraEnabled)
    {
        UpdateDebugCamera(timeElapsed);
        return;
    }

    if (m_mode == TowerDefenseMode::Playing)
    {
        XMFLOAT3 move{ 0.0f, 0.0f, 0.0f };
        if (m_topDownView)
        {
            if (GetAsyncKeyState('W') & 0x8000) move.z += 1.0f;
            if (GetAsyncKeyState('S') & 0x8000) move.z -= 1.0f;
            if (GetAsyncKeyState('D') & 0x8000) move.x += 1.0f;
            if (GetAsyncKeyState('A') & 0x8000) move.x -= 1.0f;
        }
        else if (m_camera)
        {
            XMFLOAT3 forward = m_camera->GetN();
            XMFLOAT3 right = m_camera->GetU();
            forward.y = 0.0f;
            right.y = 0.0f;
            forward = Normalize(forward);
            right = Normalize(right);

            if (GetAsyncKeyState('W') & 0x8000) move = Utiles::Vector3::Add(move, forward);
            if (GetAsyncKeyState('S') & 0x8000) move = Utiles::Vector3::Sub(move, forward);
            if (GetAsyncKeyState('D') & 0x8000) move = Utiles::Vector3::Add(move, right);
            if (GetAsyncKeyState('A') & 0x8000) move = Utiles::Vector3::Sub(move, right);
        }

        if (Utiles::Vector3::Dot(move, move) > Utiles::Physics::Epsilon)
        {
            const float speed = max(13.0f, m_cameraZoom * 0.62f);
            MoveGameplayCamera(Utiles::Vector3::Mul(Normalize(move), speed * timeElapsed));
        }
    }
}

bool TowerDefenseScene::OnKeyDown(WPARAM wParam)
{
    if (wParam == 'B')
    {
        m_rightMouseOrbiting = false;
        ToggleDebugCamera();
        return true;
    }

    if (wParam == 'V')
    {
        ToggleGameplayView();
        return true;
    }

    if (wParam == 'R' && m_mode == TowerDefenseMode::Playing && !m_shopCollapsed)
    {
        RollShopOffers(false);
        return true;
    }

    if (wParam == 'F' && m_mode == TowerDefenseMode::Playing)
    {
        m_gameSpeedIndex = (m_gameSpeedIndex + 1) % 3;
        return true;
    }

    if (wParam == VK_ESCAPE && m_mode != TowerDefenseMode::StartScreen)
    {
        BuildStartScreen();
        return true;
    }

    if (wParam == VK_RETURN && m_mode == TowerDefenseMode::StartScreen)
    {
        StartGame();
        return true;
    }

    if (m_mode == TowerDefenseMode::StartScreen)
    {
        if (wParam >= '1' && wParam <= '3')
        {
            m_selectedStage = clamp(static_cast<int>(wParam - '1'), 0, StagePresetCount - 1);
            return true;
        }
        if (wParam >= '4' && wParam <= '6')
        {
            m_selectedDifficulty = clamp(static_cast<int>(wParam - '4'), 0, DifficultyPresetCount - 1);
            return true;
        }
    }

    return false;
}

void TowerDefenseScene::MouseWheelEvent(WPARAM wParam)
{
    if (m_debugCameraEnabled)
    {
        Scene::MouseWheelEvent(wParam);
        return;
    }

    const short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (wheelDelta == 0) return;

    const float wheelSteps = static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
    m_cameraZoom = clamp(m_cameraZoom - wheelSteps * 4.5f, 18.0f, 82.0f);
    UpdateGameplayCamera(0.0f);
}

void TowerDefenseScene::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    Scene::Render(commandList);
    BeginBitmapTextPass();
    RenderStartScreenUI(commandList);
    RenderSelectedTowerRange(commandList);
    RenderMergeCandidateHighlights(commandList);
    RenderShopUI(commandList);
    RenderGoldUI(commandList);
    RenderWavePreviewUI(commandList);
    RenderTowerUpgradeUI(commandList);
    RenderMiniMapUI(commandList);
    RenderBossHealthUI(commandList);
    RenderBossIntroUI(commandList);
    RenderBossRewardUI(commandList);
    RenderWaveRewardUI(commandList);
    RenderTowerInfoUI(commandList);
    RenderDamagePopups(commandList);
    RenderResultUI(commandList);
}

void TowerDefenseScene::ReleaseObjects()
{
    m_towers.clear();
    m_generators.clear();
    m_enemies.clear();
    m_hitMarkers.clear();
    m_damagePopups.clear();
    m_rollingBoulders.clear();
    m_scopeMarkers.clear();
    m_projectiles.clear();
    ClearDragGhost();
    m_selectedTower.reset();
    m_waypoints.clear();
    m_enemyPaths.clear();
    m_terrainHeightMap.reset();
    m_stageHeightMaps.clear();
    m_stageTerrainMeshes.clear();
    m_terrainCollider.reset();
    m_terrainObject.reset();
    m_towerModelMesh.reset();
    m_boulderMesh.reset();
    m_towerModelParts.clear();
    m_shopPanel.reset();
    m_shopSlots.clear();
    m_towerInfoWidgets.clear();
    m_goldUiWidgets.clear();
    m_waveUiWidgets.clear();
    m_bossHealthWidgets.clear();
    m_hudWidgets.clear();
    m_selectedTowerRangeMarkers.clear();
    m_mergeCandidateMarkers.clear();
    m_bitmapTextRects.clear();
    m_bitmapTextCache.clear();
    m_sunObject.reset();
    m_moonObject.reset();
    m_sunLight.reset();
    m_moonLight.reset();
    m_rightMouseOrbiting = false;
    m_shopCollapsed = false;
    m_gameSpeedIndex = 0;
    m_cameraShakeTimer = 0.0f;
    m_cameraShakeDuration = 0.0f;
    m_cameraShakeIntensity = 0.0f;
    m_bossIntroTimer = 0.0f;
    m_consumableCooldown = 0.0f;
    m_waveRunning = false;
    m_bossRewardPending = false;
    m_bossRewardWave = 0;
    m_waveRewardPending = false;
    m_waveRewardWave = 0;
    for (int& level : m_towerDamageLevels) level = 0;
    m_totalEnemiesDefeated = 0;
    m_bossesDefeated = 0;
    m_towersPlaced = 0;
    m_towersMerged = 0;
    m_highestTowerTier = 1;
    m_goldEarned = 0;
    m_wavesCleared = 0;
    Scene::ReleaseObjects();

    m_startMaterial.reset();
    m_startAccentMaterial.reset();
    m_fieldMaterial.reset();
    m_blockedMaterial.reset();
    m_shopPanelMaterial.reset();
    m_shopSlotMaterial.reset();
    m_infoBarBackgroundMaterial.reset();
    m_infoDamageMaterial.reset();
    m_infoRangeMaterial.reset();
    m_infoFireRateMaterial.reset();
    m_infoCooldownMaterial.reset();
    m_healthBarBackMaterial.reset();
    m_healthBarFillMaterial.reset();
    m_deathEffectMaterial.reset();
    m_coinMaterial.reset();
    m_goldUiMaterial.reset();
    m_goldDigitMaterial.reset();
    m_bitmapTextMaterial.reset();
    m_resultVictoryMaterial.reset();
    m_resultDefeatMaterial.reset();
    m_bossMaterial.reset();
    m_flyingEnemyMaterial.reset();
    m_runnerEnemyMaterial.reset();
    m_armoredEnemyMaterial.reset();
    m_splitterEnemyMaterial.reset();
    m_bossHealthFillMaterial.reset();
    m_lifeUiMaterial.reset();
    m_mergeHighlightMaterial.reset();
    m_meteorMaterial.reset();
    m_meteorUiMaterial.reset();
    m_freezeMaterial.reset();
    m_freezeUiMaterial.reset();
    m_boostMaterial.reset();
    m_boostUiMaterial.reset();
    m_boulderMaterial.reset();
    m_boulderUiMaterial.reset();
    m_generatorMaterial.reset();
    m_generatorUiMaterial.reset();
    m_shopConsumableSlotMaterial.reset();
    m_shopGeneratorSlotMaterial.reset();
    for (auto& materialRow : m_shopTowerMaterials)
    {
        for (auto& material : materialRow) material.reset();
    }
    for (auto& materialRow : m_towerMaterials)
    {
        for (auto& material : materialRow) material.reset();
    }
    for (auto& materialRow : m_towerModelMaterials)
    {
        for (auto& material : materialRow) material.reset();
    }
    for (auto& material : m_projectileMaterials) material.reset();
    for (auto& material : m_hitMaterials) material.reset();
    m_dragGhostMaterial.reset();
    m_enemyMaterial.reset();
    m_hitMaterial.reset();
    m_scopeMaterial.reset();
    m_projectileMaterial.reset();
    m_tunnelOpeningMaterial.reset();
    m_tunnelStoneMaterial.reset();
    m_sunMaterial.reset();
    m_moonMaterial.reset();
    ReleaseBitmapFont();
}

void TowerDefenseScene::ReleaseUploadBuffer()
{
    Scene::ReleaseUploadBuffer();
}

shared_ptr<GameObject> TowerDefenseScene::CreateCubeObject(const string& name,
    const XMFLOAT3& position,
    const XMFLOAT3& scale,
    const shared_ptr<Material>& material) const
{
    auto object = make_shared<GameObject>();
    object->SetName(name);
    object->SetMesh(m_cube);
    object->SetMaterial(material);

    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixScaling(scale.x, scale.y, scale.z) *
        XMMatrixTranslation(position.x, position.y, position.z));
    object->SetWorldMatrix(world);
    return object;
}

shared_ptr<GameObject> TowerDefenseScene::CreateTowerObject(const string& name,
    const XMFLOAT3& terrainPoint,
    TowerDefenseTowerType type,
    int tier) const
{
    tier = clamp(tier, 1, MaxTowerTier);
    const int typeIndex = TowerTypeIndex(type);

    if (!m_towerModelMesh)
    {
        const XMFLOAT3 center{
            terrainPoint.x,
            TerrainHeightAtWorldXZ(terrainPoint.x, terrainPoint.z) + TowerHalfHeight,
            terrainPoint.z
        };
        return CreateCubeObject(name,
            center,
            TowerVisualSize(type, tier),
            m_towerMaterials[typeIndex][tier - 1]);
    }

    auto object = make_shared<GameObject>();
    object->SetName(name);
    object->SetMesh(m_towerModelMesh);
    object->SetMaterial(m_towerModelMaterials[typeIndex][tier - 1]
        ? m_towerModelMaterials[typeIndex][tier - 1]
        : m_towerMaterials[typeIndex][tier - 1]);

    object->SetWorldMatrix(BuildTowerModelRootMatrix(terrainPoint, type, tier));
    return object;
}

shared_ptr<GameObject> TowerDefenseScene::CreateCapsuleObject(const string& name,
    const XMFLOAT3& position,
    const shared_ptr<Material>& material,
    float scale) const
{
    auto object = make_shared<GameObject>();
    object->SetName(name);
    object->SetMesh(m_capsuleMesh);
    object->SetMaterial(material);

    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixScaling(scale, scale, scale) *
        XMMatrixTranslation(position.x, position.y, position.z));
    object->SetWorldMatrix(world);
    return object;
}

bool TowerDefenseScene::TryGetGroundPoint(HWND hWnd, XMFLOAT3& outPoint) const
{
    return TryGetPointOnPlane(hWnd, 0.0f, outPoint);
}

bool TowerDefenseScene::TryGetPointOnPlane(HWND hWnd, float planeY, XMFLOAT3& outPoint) const
{
    XMFLOAT3 rayOrigin{};
    XMFLOAT3 rayDirection{};
    if (!BuildMouseRay(hWnd, m_camera, CameraFovY, rayOrigin, rayDirection)) return false;
    if (fabsf(rayDirection.y) <= 0.0001f) return false;

    const float t = (planeY - rayOrigin.y) / rayDirection.y;
    if (t < 0.0f) return false;

    outPoint = Utiles::Vector3::Add(rayOrigin, Utiles::Vector3::Mul(rayDirection, t));
    return true;
}

bool TowerDefenseScene::TryGetTerrainPoint(HWND hWnd, XMFLOAT3& outPoint) const
{
    if (!m_terrainCollider) return false;

    XMFLOAT3 rayOrigin{};
    XMFLOAT3 rayDirection{};
    if (!BuildMouseRay(hWnd, m_camera, CameraFovY, rayOrigin, rayDirection)) return false;
    float hitDistance = 0.0f;
    if (!m_terrainCollider->Raycast(rayOrigin, rayDirection, hitDistance)) return false;

    outPoint = Utiles::Vector3::Add(rayOrigin, Utiles::Vector3::Mul(rayDirection, hitDistance));
    return true;
}

bool TowerDefenseScene::TryGetShopSlotFromCursor(HWND hWnd, int& outSlot) const
{
    if (m_mode != TowerDefenseMode::Playing || m_shopCollapsed) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    for (int slot = 1; slot <= 3; ++slot)
    {
        XMFLOAT2 center{};
        XMFLOAT2 halfSize{};
        if (!GetShopSlotRect(slot, width, height, center, halfSize)) continue;

        if (fabsf(static_cast<float>(cursor.x) - center.x) <= halfSize.x &&
            fabsf(static_cast<float>(cursor.y) - center.y) <= halfSize.y)
        {
            outSlot = slot;
            return true;
        }
    }

    return false;
}

bool TowerDefenseScene::TryHandleStartMenuClick(HWND hWnd)
{
    if (m_mode != TowerDefenseMode::StartScreen) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    const float x = static_cast<float>(cursor.x) / width;
    const float y = static_cast<float>(cursor.y) / height;
    auto hitRect = [x, y](const XMFLOAT2& center, const XMFLOAT2& halfSize)
        {
            return fabsf(x - center.x) <= halfSize.x && fabsf(y - center.y) <= halfSize.y;
        };

    const float stageCenters[StagePresetCount]{ 0.345f, 0.500f, 0.655f };
    for (int stage = 0; stage < StagePresetCount; ++stage)
    {
        if (hitRect(XMFLOAT2{ stageCenters[stage], 0.448f }, XMFLOAT2{ 0.066f, 0.035f }))
        {
            m_selectedStage = stage;
            return true;
        }
    }

    const float difficultyCenters[DifficultyPresetCount]{ 0.345f, 0.500f, 0.655f };
    for (int difficulty = 0; difficulty < DifficultyPresetCount; ++difficulty)
    {
        if (hitRect(XMFLOAT2{ difficultyCenters[difficulty], 0.580f }, XMFLOAT2{ 0.066f, 0.035f }))
        {
            m_selectedDifficulty = difficulty;
            return true;
        }
    }

    if (hitRect(XMFLOAT2{ 0.5f, 0.805f }, XMFLOAT2{ 0.110f, 0.040f }))
    {
        StartGame();
        return true;
    }

    XMFLOAT3 point{};
    if (TryGetGroundPoint(hWnd, point) &&
        fabsf(point.x) <= 2.15f && fabsf(point.z) <= 1.05f)
    {
        StartGame();
        return true;
    }

    return false;
}

bool TowerDefenseScene::CanPlaceTower(const XMFLOAT3& terrainPoint) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_terrainCollider) return false;

    float terrainHeight = 0.0f;
    XMFLOAT3 terrainNormal{};
    if (!m_terrainCollider->GetHeightAtWorld(terrainPoint, terrainHeight, terrainNormal)) return false;
    if (terrainNormal.y < 0.58f) return false;
    if (IsNearPath(terrainPoint, PathBlockRadius)) return false;
    if (HasTowerNear(terrainPoint, TowerSpacing)) return false;
    return true;
}

bool TowerDefenseScene::CanMergeOfferAtPoint(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer) const
{
    if (offer.kind != TowerDefenseOfferKind::Tower || m_gold < offer.cost) return false;

    const TowerDefenseTower* target = nullptr;
    float bestDistanceSq = TowerPickRadius * TowerPickRadius;
    for (const auto& tower : m_towers)
    {
        if (!tower.object) continue;
        const float distanceSq = DistanceSqXZ(tower.position, terrainPoint);
        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            target = &tower;
        }
    }

    return target &&
        target->type == offer.type &&
        target->tier == offer.tier &&
        target->tier < MaxTowerTier;
}

bool TowerDefenseScene::CanMergeDraggedTowerAtPoint(const XMFLOAT3& terrainPoint) const
{
    auto sourceObject = m_dragSourceTower.lock();
    if (!sourceObject) return false;

    const TowerDefenseTower* source = nullptr;
    const TowerDefenseTower* target = nullptr;
    float bestDistanceSq = TowerPickRadius * TowerPickRadius;

    for (const auto& tower : m_towers)
    {
        if (!tower.object) continue;
        if (tower.object == sourceObject) source = &tower;

        const float distanceSq = DistanceSqXZ(tower.position, terrainPoint);
        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            target = &tower;
        }
    }

    if (!source || !target || !target->object || target->object == sourceObject) return false;
    return source->type == target->type && source->tier == target->tier && target->tier < MaxTowerTier;
}

bool TowerDefenseScene::IsNearPath(const XMFLOAT3& terrainPoint, float radius) const
{
    const float radiusSq = radius * radius;
    auto isNearPathSegment = [terrainPoint, radiusSq](const vector<XMFLOAT3>& path)
        {
            if (path.size() < 2) return false;

            for (size_t i = 1; i < path.size(); ++i)
            {
                XMVECTOR a = XMVectorSet(path[i - 1].x, 0.0f, path[i - 1].z, 0.0f);
                XMVECTOR b = XMVectorSet(path[i].x, 0.0f, path[i].z, 0.0f);
                XMVECTOR p = XMVectorSet(terrainPoint.x, 0.0f, terrainPoint.z, 0.0f);
                XMVECTOR ab = XMVectorSubtract(b, a);
                float abLengthSq = XMVectorGetX(XMVector3Dot(ab, ab));
                if (abLengthSq <= Utiles::Physics::Epsilon) continue;

                float t = XMVectorGetX(XMVector3Dot(XMVectorSubtract(p, a), ab)) / abLengthSq;
                t = clamp(t, 0.0f, 1.0f);
                XMVECTOR closest = XMVectorAdd(a, XMVectorScale(ab, t));
                XMVECTOR delta = XMVectorSubtract(p, closest);
                if (XMVectorGetX(XMVector3Dot(delta, delta)) <= radiusSq) return true;
            }

            return false;
        };

    for (const auto& path : m_enemyPaths)
    {
        if (isNearPathSegment(path)) return true;
    }

    if (m_enemyPaths.empty() && isNearPathSegment(m_waypoints)) return true;

    return false;
}

bool TowerDefenseScene::HasTowerNear(const XMFLOAT3& terrainPoint, float radius) const
{
    const float radiusSq = radius * radius;
    if (any_of(m_towers.begin(), m_towers.end(), [terrainPoint, radiusSq](const TowerDefenseTower& tower)
        {
            return DistanceSqXZ(tower.position, terrainPoint) <= radiusSq;
        }))
    {
        return true;
    }

    return any_of(m_generators.begin(), m_generators.end(), [terrainPoint, radiusSq](const TowerDefenseGenerator& generator)
        {
            return DistanceSqXZ(generator.position, terrainPoint) <= radiusSq;
        });
}

TowerDefenseTower* TowerDefenseScene::FindTowerAtPoint(const XMFLOAT3& terrainPoint)
{
    TowerDefenseTower* selected = nullptr;
    float bestDistanceSq = TowerPickRadius * TowerPickRadius;

    for (auto& tower : m_towers)
    {
        if (!tower.object) continue;

        const float distanceSq = DistanceSqXZ(tower.position, terrainPoint);
        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            selected = &tower;
        }
    }

    return selected;
}

int TowerDefenseScene::FindTowerIndexByObject(const shared_ptr<GameObject>& object) const
{
    if (!object) return -1;

    for (int i = 0; i < static_cast<int>(m_towers.size()); ++i)
    {
        if (m_towers[i].object == object) return i;
        if (any_of(m_towers[i].parts.begin(), m_towers[i].parts.end(),
            [&object](const TowerDefenseTowerPartInstance& instance)
            {
                return instance.object == object;
            }))
        {
            return i;
        }
    }

    return -1;
}

bool TowerDefenseScene::IsEnemyInTowerRange(const TowerDefenseTower& tower, const TowerDefenseEnemy& enemy) const
{
    if (!tower.object || !enemy.object || enemy.health <= 0.0f) return false;
    if (enemy.isFlying && !tower.targetsAir) return false;
    if (!enemy.isFlying && !tower.targetsGround) return false;

    const float distanceSq = DistanceSqXZ(tower.object->GetPosition(), enemy.object->GetPosition());
    if (tower.minRange > 0.0f && distanceSq < tower.minRange * tower.minRange) return false;
    const float rangeSq = tower.range * tower.range;
    return distanceSq <= rangeSq;
}

TowerDefenseEnemy* TowerDefenseScene::FindEnemyByObject(const shared_ptr<GameObject>& object)
{
    if (!object) return nullptr;

    for (auto& enemy : m_enemies)
    {
        if (enemy.object == object) return &enemy;
    }

    return nullptr;
}

const TowerDefenseTower* TowerDefenseScene::GetSelectedTower() const
{
    auto selectedObject = m_selectedTower.lock();
    if (!selectedObject) return nullptr;

    for (const auto& tower : m_towers)
    {
        if (tower.object == selectedObject) return &tower;
        if (any_of(tower.parts.begin(), tower.parts.end(),
            [&selectedObject](const TowerDefenseTowerPartInstance& instance)
            {
                return instance.object == selectedObject;
            }))
        {
            return &tower;
        }
    }

    return nullptr;
}

const TowerDefenseEnemy* TowerDefenseScene::GetActiveBoss() const
{
    for (const auto& enemy : m_enemies)
    {
        if (enemy.isBoss && enemy.object && enemy.health > 0.0f) return &enemy;
    }

    return nullptr;
}

int TowerDefenseScene::GetWaveSize(int wave) const
{
    wave = max(1, wave);
    return 8 + wave * 3 + (wave >= 4 ? 2 : 0);
}

bool TowerDefenseScene::IsBossWave(int wave) const
{
    return wave > 0 && (wave % 3 == 0 || wave == MaxWave);
}

int TowerDefenseScene::GetRerollCost() const
{
    return 1 + max(0, m_wave - 1) / 3;
}

wstring TowerDefenseScene::ResolveBitmapFontPath() const
{
    auto fileExists = [](const wstring& path)
        {
            DWORD attributes = GetFileAttributesW(path.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        };

    auto parentPath = [](const wstring& path)
        {
            const size_t slash = path.find_last_of(L"\\/");
            return slash == wstring::npos ? wstring{} : path.substr(0, slash);
        };

    vector<wstring> roots;
    WCHAR currentDirectory[MAX_PATH]{};
    if (GetCurrentDirectoryW(_countof(currentDirectory), currentDirectory) > 0)
    {
        roots.push_back(currentDirectory);
    }

    WCHAR modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, _countof(modulePath)) > 0)
    {
        roots.push_back(parentPath(modulePath));
    }

    for (size_t i = 0; i < roots.size(); ++i)
    {
        wstring root = roots[i];
        for (int depth = 0; depth < 6 && !root.empty(); ++depth)
        {
            const wstring direct = root + L"\\TerrarumSansBitmap.otf";
            if (fileExists(direct)) return direct;

            const wstring project = root + L"\\GameJam\\TerrarumSansBitmap.otf";
            if (fileExists(project)) return project;

            root = parentPath(root);
        }
    }

    return {};
}

const TowerDefenseTextCache& TowerDefenseScene::GetBitmapTextCache(const wstring& text, int pixelHeight) const
{
    for (const auto& cache : m_bitmapTextCache)
    {
        if (cache.pixelHeight == pixelHeight && cache.text == text) return cache;
    }

    m_bitmapTextCache.push_back(RasterizeBitmapText(text, pixelHeight));
    return m_bitmapTextCache.back();
}

TowerDefenseTextCache TowerDefenseScene::RasterizeBitmapText(const wstring& text, int pixelHeight) const
{
    TowerDefenseTextCache cache{};
    cache.text = text;
    cache.pixelHeight = max(8, pixelHeight);

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    ReleaseDC(nullptr, screenDc);
    if (!memoryDc) return cache;

    HFONT font = CreateFontW(
        -cache.pixelHeight,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        m_bitmapFontFamily.c_str());
    HGDIOBJ oldFont = font ? SelectObject(memoryDc, font) : nullptr;

    RECT measure{ 0, 0, 1, 1 };
    DrawTextW(memoryDc, text.c_str(), -1, &measure,
        DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT | DT_NOPREFIX);

    const int width = max(1, measure.right - measure.left + 2);
    const int height = max(1, measure.bottom - measure.top + 2);
    cache.width = static_cast<float>(width);
    cache.height = static_cast<float>(height);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBitmap = bitmap ? SelectObject(memoryDc, bitmap) : nullptr;
    if (bitmap && bits)
    {
        memset(bits, 0, static_cast<size_t>(width) * height * 4);
        SetBkMode(memoryDc, TRANSPARENT);
        SetTextColor(memoryDc, RGB(255, 255, 255));

        RECT drawRect{ 1, 1, width, height };
        DrawTextW(memoryDc, text.c_str(), -1, &drawRect,
            DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);

        const UINT32* pixels = reinterpret_cast<const UINT32*>(bits);
        for (int y = 0; y < height; ++y)
        {
            int x = 0;
            while (x < width)
            {
                while (x < width && (pixels[static_cast<size_t>(y) * width + x] & 0x00ffffffu) == 0u) ++x;
                const int start = x;
                while (x < width && (pixels[static_cast<size_t>(y) * width + x] & 0x00ffffffu) != 0u) ++x;
                const int runLength = x - start;
                if (runLength > 0)
                {
                    cache.runs.push_back(TowerDefenseTextRun{
                        XMFLOAT2{ static_cast<float>(start) + static_cast<float>(runLength) * 0.5f, static_cast<float>(y) + 0.5f },
                        XMFLOAT2{ static_cast<float>(runLength), 1.0f }
                        });
                }
            }
        }
    }

    if (oldBitmap) SelectObject(memoryDc, oldBitmap);
    if (bitmap) DeleteObject(bitmap);
    if (oldFont) SelectObject(memoryDc, oldFont);
    if (font) DeleteObject(font);
    DeleteDC(memoryDc);

    return cache;
}

TowerDefenseEnemy* TowerDefenseScene::AcquireTowerTarget(const TowerDefenseTower& tower)
{
    TowerDefenseEnemy* bestTarget = nullptr;
    float bestScore = tower.range * tower.range;
    const XMFLOAT3 towerPosition = tower.object ? tower.object->GetPosition() : tower.position;

    for (auto& enemy : m_enemies)
    {
        if (!enemy.object || enemy.health <= 0.0f) continue;
        if (!IsEnemyInTowerRange(tower, enemy)) continue;

        float distanceSq = DistanceSqXZ(towerPosition, enemy.object->GetPosition());
        float score = distanceSq;
        if (tower.type == TowerDefenseTowerType::Flak && enemy.isFlying) score *= 0.28f;
        if (tower.type == TowerDefenseTowerType::Mortar) score = -distanceSq;
        if (!bestTarget || score <= bestScore)
        {
            bestScore = score;
            bestTarget = &enemy;
        }
    }

    return bestTarget;
}

XMFLOAT3 TowerDefenseScene::TerrainWorldPosition(float localX, float localZ, float yOffset) const
{
    if (!m_terrainHeightMap) return XMFLOAT3{ 0.0f, yOffset, 0.0f };

    localX = clamp(localX, 0.0f, m_terrainHeightMap->GetWorldWidth());
    localZ = clamp(localZ, 0.0f, m_terrainHeightMap->GetWorldLength());
    return XMFLOAT3{
        localX - m_terrainHeightMap->GetWorldWidth() * 0.5f,
        m_terrainHeightMap->SampleHeight(localX, localZ) + yOffset,
        localZ - m_terrainHeightMap->GetWorldLength() * 0.5f
    };
}

float TowerDefenseScene::TerrainHeightAtWorldXZ(float x, float z) const
{
    if (!m_terrainHeightMap) return 0.0f;

    float localX = x + m_terrainHeightMap->GetWorldWidth() * 0.5f;
    float localZ = z + m_terrainHeightMap->GetWorldLength() * 0.5f;
    return m_terrainHeightMap->SampleHeight(localX, localZ);
}

void TowerDefenseScene::PositionScopeMarker(TowerDefenseScopeMarker& marker) const
{
    auto target = marker.target.lock();
    if (!target || marker.objects.size() < 5) return;

    XMFLOAT3 center = target->GetPosition();
    center.y += max(0.42f, marker.radius * 0.92f);

    const float r = marker.radius;
    marker.objects[0]->SetPosition(XMFLOAT3{ center.x - r, center.y, center.z });
    marker.objects[1]->SetPosition(XMFLOAT3{ center.x + r, center.y, center.z });
    marker.objects[2]->SetPosition(XMFLOAT3{ center.x, center.y, center.z + r });
    marker.objects[3]->SetPosition(XMFLOAT3{ center.x, center.y, center.z - r });
    marker.objects[4]->SetPosition(center);
}

bool TowerDefenseScene::GetShopSlotRect(int tier, float width, float height,
    XMFLOAT2& outCenter, XMFLOAT2& outHalfSize) const
{
    if (width <= 1.0f || height <= 1.0f) return false;

    tier = clamp(tier, 1, 3);
    const float spacing = max(74.0f, width * 0.085f);
    outCenter = XMFLOAT2{
        width * 0.5f + (static_cast<float>(tier) - 2.0f) * spacing,
        height * 0.865f
    };
    outHalfSize = XMFLOAT2{
        max(42.0f, width * 0.036f),
        max(38.0f, height * 0.052f)
    };
    return true;
}

bool TowerDefenseScene::IsTowerInfoPanelUnderCursor(HWND hWnd) const
{
    if (!GetSelectedTower()) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    const XMFLOAT2 panelCenter{ width * 0.835f, height * 0.150f };
    const XMFLOAT2 panelHalfSize{ width * 0.145f, height * 0.118f };
    return fabsf(static_cast<float>(cursor.x) - panelCenter.x) <= panelHalfSize.x &&
        fabsf(static_cast<float>(cursor.y) - panelCenter.y) <= panelHalfSize.y;
}

bool TowerDefenseScene::IsWaveToggleUnderCursor(HWND hWnd) const
{
    if (m_mode != TowerDefenseMode::Playing || m_bossRewardPending || m_waveRewardPending) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    const XMFLOAT2 center{ width * 0.500f, height * 0.083f };
    const XMFLOAT2 halfSize{ max(50.0f, width * 0.053f), max(24.0f, height * 0.022f) };
    return fabsf(static_cast<float>(cursor.x) - center.x) <= halfSize.x &&
        fabsf(static_cast<float>(cursor.y) - center.y) <= halfSize.y;
}

bool TowerDefenseScene::IsShopToggleUnderCursor(HWND hWnd) const
{
    if (m_mode != TowerDefenseMode::Playing || m_bossRewardPending || m_waveRewardPending) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    const XMFLOAT2 center{
        width * 0.500f,
        height * (m_shopCollapsed ? 0.945f : 0.760f)
    };
    const XMFLOAT2 halfSize{
        max(48.0f, width * 0.055f),
        max(20.0f, height * 0.023f)
    };
    return fabsf(static_cast<float>(cursor.x) - center.x) <= halfSize.x &&
        fabsf(static_cast<float>(cursor.y) - center.y) <= halfSize.y;
}

bool TowerDefenseScene::TryGetTowerUpgradeChoiceFromCursor(HWND hWnd, int& outTypeIndex) const
{
    if (m_mode != TowerDefenseMode::Playing || m_bossRewardPending || m_waveRewardPending) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    for (int type = 0; type < TowerTypeCount; ++type)
    {
        const int column = type % 3;
        const int row = type / 3;
        const XMFLOAT2 center{
            width * (0.059f + static_cast<float>(column) * 0.073f),
            height * (0.785f + static_cast<float>(row) * 0.108f)
        };
        const XMFLOAT2 halfSize{
            max(26.0f, width * 0.030f),
            max(24.0f, height * 0.030f)
        };
        if (fabsf(static_cast<float>(cursor.x) - center.x) <= halfSize.x &&
            fabsf(static_cast<float>(cursor.y) - center.y) <= halfSize.y)
        {
            outTypeIndex = type;
            return true;
        }
    }

    return false;
}

void TowerDefenseScene::ToggleWaveRunning()
{
    if (m_mode != TowerDefenseMode::Playing || m_bossRewardPending || m_waveRewardPending) return;

    const int waveSize = GetWaveSize(m_wave);
    if (m_spawnedInWave >= waveSize) return;

    m_waveRunning = !m_waveRunning;
    if (m_waveRunning && m_spawnTimer <= 0.0f)
    {
        m_spawnTimer = 0.02f;
    }
}

void TowerDefenseScene::ToggleShopCollapsed()
{
    if (m_mode != TowerDefenseMode::Playing) return;

    m_shopCollapsed = !m_shopCollapsed;
    if (m_shopCollapsed) ClearDragGhost();
}

int TowerDefenseScene::GetTowerDamageUpgradeCost(int typeIndex) const
{
    typeIndex = clamp(typeIndex, 0, TowerTypeCount - 1);
    const int level = clamp(m_towerDamageLevels[typeIndex], 0, MaxTowerDamageUpgradeLevel);
    return 8 + typeIndex * 2 + level * 7;
}

float TowerDefenseScene::GetTowerDamageMultiplier(TowerDefenseTowerType type) const
{
    const int typeIndex = TowerTypeIndex(type);
    const int level = clamp(m_towerDamageLevels[typeIndex], 0, MaxTowerDamageUpgradeLevel);
    return 1.0f + static_cast<float>(level) * 0.16f;
}

float TowerDefenseScene::GetGameSpeedMultiplier() const
{
    switch (clamp(m_gameSpeedIndex, 0, 2))
    {
    case 1:
        return 2.0f;
    case 2:
        return 3.0f;
    default:
        return 1.0f;
    }
}

float TowerDefenseScene::GetDifficultyHealthMultiplier() const
{
    static constexpr float Values[DifficultyPresetCount]{ 0.82f, 1.0f, 1.30f };
    return Values[clamp(m_selectedDifficulty, 0, DifficultyPresetCount - 1)];
}

float TowerDefenseScene::GetDifficultySpeedMultiplier() const
{
    static constexpr float Values[DifficultyPresetCount]{ 0.92f, 1.0f, 1.13f };
    return Values[clamp(m_selectedDifficulty, 0, DifficultyPresetCount - 1)];
}

float TowerDefenseScene::GetDifficultyRewardMultiplier() const
{
    static constexpr float Values[DifficultyPresetCount]{ 1.05f, 1.0f, 1.28f };
    return Values[clamp(m_selectedDifficulty, 0, DifficultyPresetCount - 1)];
}

int TowerDefenseScene::ScaleReward(int amount) const
{
    if (amount <= 0) return 0;

    return max(1, static_cast<int>(floorf(static_cast<float>(amount) * GetDifficultyRewardMultiplier() + 0.5f)));
}

bool TowerDefenseScene::TryUpgradeTowerDamage(int typeIndex)
{
    if (m_mode != TowerDefenseMode::Playing) return false;
    typeIndex = clamp(typeIndex, 0, TowerTypeCount - 1);
    if (m_towerDamageLevels[typeIndex] >= MaxTowerDamageUpgradeLevel) return false;

    const int cost = GetTowerDamageUpgradeCost(typeIndex);
    if (m_gold < cost) return false;

    m_gold -= cost;
    ++m_towerDamageLevels[typeIndex];
    const TowerDefenseTowerType upgradedType = TowerTypeFromIndex(typeIndex);
    for (auto& tower : m_towers)
    {
        if (tower.type == upgradedType) ApplyTowerStats(tower);
    }
    return true;
}

bool TowerDefenseScene::TryGetBossRewardChoiceFromCursor(HWND hWnd, int& outChoice) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_bossRewardPending) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    const float centers[3]{ 0.330f, 0.500f, 0.670f };
    const XMFLOAT2 halfSize{ max(72.0f, width * 0.072f), max(50.0f, height * 0.062f) };
    for (int i = 0; i < 3; ++i)
    {
        const XMFLOAT2 center{ width * centers[i], height * 0.462f };
        if (fabsf(static_cast<float>(cursor.x) - center.x) <= halfSize.x &&
            fabsf(static_cast<float>(cursor.y) - center.y) <= halfSize.y)
        {
            outChoice = i + 1;
            return true;
        }
    }

    return false;
}

void TowerDefenseScene::ApplyBossRewardChoice(int choice)
{
    if (!m_bossRewardPending) return;

    choice = clamp(choice, 1, 3);
    const int rewardWave = max(1, m_bossRewardWave);
    switch (choice)
    {
    case 1:
    {
        const int bonusGold = ScaleReward(14 + rewardWave * 3);
        AddGold(bonusGold);
        XMFLOAT3 position = m_cameraFocus;
        position.y = TerrainHeightAtWorldXZ(position.x, position.z) + 0.75f;
        SpawnCoinDropEffect(position, bonusGold);
        break;
    }
    case 2:
        m_lives = min(m_maxLives, m_lives + 5);
        break;
    default:
        RollShopOffers(true);
        for (auto& offer : m_shopOffers)
        {
            offer.tier = clamp(offer.tier + 1, 1, MaxTowerTier);
            offer.cost += 2;
        }
        break;
    }

    m_bossRewardPending = false;
    m_bossRewardWave = 0;
}

void TowerDefenseScene::ShowBossReward()
{
    if (m_bossRewardPending) return;

    m_bossRewardPending = true;
    m_bossRewardWave = m_wave;
    m_waveRunning = false;
    ClearDragGhost();
}

bool TowerDefenseScene::TryGetWaveRewardChoiceFromCursor(HWND hWnd, int& outChoice) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_waveRewardPending) return false;

    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return false;

    const float centers[3]{ 0.330f, 0.500f, 0.670f };
    const XMFLOAT2 halfSize{ max(72.0f, width * 0.072f), max(50.0f, height * 0.062f) };
    for (int i = 0; i < 3; ++i)
    {
        const XMFLOAT2 center{ width * centers[i], height * 0.462f };
        if (fabsf(static_cast<float>(cursor.x) - center.x) <= halfSize.x &&
            fabsf(static_cast<float>(cursor.y) - center.y) <= halfSize.y)
        {
            outChoice = i + 1;
            return true;
        }
    }

    return false;
}

void TowerDefenseScene::ApplyWaveRewardChoice(int choice)
{
    if (!m_waveRewardPending) return;

    choice = clamp(choice, 1, 3);
    const int rewardWave = max(1, m_waveRewardWave);
    switch (choice)
    {
    case 1:
    {
        const int bonusGold = ScaleReward(6 + rewardWave * 2);
        AddGold(bonusGold);
        XMFLOAT3 position = m_cameraFocus;
        position.y = TerrainHeightAtWorldXZ(position.x, position.z) + 0.70f;
        SpawnCoinDropEffect(position, bonusGold);
        break;
    }
    case 2:
        m_lives = min(m_maxLives, m_lives + 2);
        break;
    default:
        RollShopOffers(true);
        break;
    }

    m_waveRewardPending = false;
    m_waveRewardWave = 0;
    AdvanceToNextWave();
}

void TowerDefenseScene::ShowWaveReward()
{
    if (m_waveRewardPending) return;

    m_waveRewardPending = true;
    m_waveRewardWave = m_wave;
    m_waveRunning = false;
    ClearDragGhost();
}

void TowerDefenseScene::AdvanceToNextWave()
{
    if (m_wave >= MaxWave) return;

    ++m_wave;
    m_spawnedInWave = 0;
    m_spawnTimer = 0.0f;
    m_waveRunning = false;
    AddGold(ScaleReward(2));
}

void TowerDefenseScene::AddGold(int amount)
{
    if (amount <= 0) return;

    m_gold += amount;
    m_goldEarned += amount;
}

XMFLOAT4X4 TowerDefenseScene::BuildCameraAnchoredUiMatrix(const XMFLOAT2& normalizedCenter,
    const XMFLOAT2& normalizedSize,
    float depth,
    float thickness) const
{
    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixIdentity());
    if (!m_camera || !g_framework) return world;

    depth = max(0.2f, depth);
    const float aspect = max(0.1f, g_framework->GetAspectRatio());
    const float viewHeight = 2.0f * depth * tanf(CameraFovY * 0.5f);
    const float viewWidth = viewHeight * aspect;
    const float xOffset = (normalizedCenter.x - 0.5f) * viewWidth;
    const float yOffset = (0.5f - normalizedCenter.y) * viewHeight;

    const XMFLOAT3 right = m_camera->GetU();
    const XMFLOAT3 up = m_camera->GetV();
    const XMFLOAT3 forward = m_camera->GetN();
    XMFLOAT3 position = Utiles::Vector3::Add(
        m_camera->GetEye(),
        Utiles::Vector3::Add(
            Utiles::Vector3::Mul(forward, depth),
            Utiles::Vector3::Add(
                Utiles::Vector3::Mul(right, xOffset),
                Utiles::Vector3::Mul(up, yOffset))));

    const float scaleX = max(0.001f, viewWidth * normalizedSize.x * 0.5f);
    const float scaleY = max(0.001f, viewHeight * normalizedSize.y * 0.5f);
    const float scaleZ = max(0.001f, thickness);

    XMMATRIX orientation = XMMatrixSet(
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    XMStoreFloat4x4(&world,
        XMMatrixScaling(scaleX, scaleY, scaleZ) *
        orientation *
        XMMatrixTranslation(position.x, position.y, position.z));
    return world;
}

XMFLOAT4X4 TowerDefenseScene::BuildCameraAnchoredModelMatrix(const XMFLOAT2& normalizedCenter,
    float normalizedHeight,
    float depth,
    float yawRadians) const
{
    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixIdentity());
    if (!m_camera || !g_framework || !m_towerModelMesh) return world;

    depth = max(0.2f, depth);
    normalizedHeight = max(0.001f, normalizedHeight);

    const float aspect = max(0.1f, g_framework->GetAspectRatio());
    const float viewHeight = 2.0f * depth * tanf(CameraFovY * 0.5f);
    const float viewWidth = viewHeight * aspect;
    const float xOffset = (normalizedCenter.x - 0.5f) * viewWidth;
    const float yOffset = (0.5f - normalizedCenter.y) * viewHeight;

    const XMFLOAT3 right = m_camera->GetU();
    const XMFLOAT3 up = m_camera->GetV();
    const XMFLOAT3 forward = m_camera->GetN();
    const XMFLOAT3 position = Utiles::Vector3::Add(
        m_camera->GetEye(),
        Utiles::Vector3::Add(
            Utiles::Vector3::Mul(forward, depth),
            Utiles::Vector3::Add(
                Utiles::Vector3::Mul(right, xOffset),
                Utiles::Vector3::Mul(up, yOffset))));

    const BoundingBox bounds = m_towerModelMesh->GetLocalAABB();
    const float localHeight = max(0.001f, bounds.Extents.y * 2.0f);
    const float localFootprint = max(0.001f, max(bounds.Extents.x, bounds.Extents.z) * 2.0f);
    const float targetHeight = viewHeight * normalizedHeight;
    const float targetFootprint = viewWidth * normalizedHeight * 0.62f;
    const float modelScale = min(targetHeight / localHeight, targetFootprint / localFootprint);

    XMMATRIX orientation = XMMatrixSet(
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    XMStoreFloat4x4(&world,
        XMMatrixTranslation(-bounds.Center.x, -bounds.Center.y, -bounds.Center.z) *
        XMMatrixRotationY(yawRadians) *
        XMMatrixScaling(modelScale, modelScale, modelScale) *
        orientation *
        XMMatrixTranslation(position.x, position.y, position.z));
    return world;
}

void TowerDefenseScene::RenderTowerModelIcon(const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const shared_ptr<GameObject>& object,
    TowerDefenseTowerType type,
    int tier,
    const XMFLOAT2& normalizedCenter,
    float normalizedHeight,
    float depth) const
{
    if (!object || !m_towerModelMesh) return;

    tier = clamp(tier, 1, MaxTowerTier);
    const int typeIndex = TowerTypeIndex(type);
    object->SetMesh(m_towerModelMesh);
    object->SetMaterial(m_towerModelMaterials[typeIndex][tier - 1]
        ? m_towerModelMaterials[typeIndex][tier - 1]
        : m_shopTowerMaterials[typeIndex][tier - 1]);
    object->SetWorldMatrix(BuildCameraAnchoredModelMatrix(
        normalizedCenter,
        normalizedHeight,
        depth,
        XM_PIDIV4));
    object->Render(commandList);
}

void TowerDefenseScene::RenderTierMarkers(const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const XMFLOAT2& normalizedCenter,
    int tier,
    float markerSize,
    float spacing,
    float depth,
    size_t& widgetIndex) const
{
    if (m_hudWidgets.empty()) return;

    tier = clamp(tier, 1, MaxTowerTier);
    const float startX = normalizedCenter.x - spacing * static_cast<float>(tier - 1) * 0.5f;
    const XMFLOAT2 offsets[5]{
        XMFLOAT2{ 0.0f, 0.0f },
        XMFLOAT2{ 0.0f, -markerSize * 0.72f },
        XMFLOAT2{ markerSize * 0.72f, 0.0f },
        XMFLOAT2{ 0.0f, markerSize * 0.72f },
        XMFLOAT2{ -markerSize * 0.72f, 0.0f }
    };

    for (int star = 0; star < tier; ++star)
    {
        const XMFLOAT2 starCenter{
            startX + spacing * static_cast<float>(star),
            normalizedCenter.y
        };

        for (int piece = 0; piece < 5; ++piece)
        {
            if (widgetIndex >= m_hudWidgets.size() || !m_hudWidgets[widgetIndex]) return;

            auto& widget = m_hudWidgets[widgetIndex++];
            const float pieceSize = piece == 0 ? markerSize : markerSize * 0.52f;
            widget->SetMesh(m_cube);
            widget->SetMaterial(m_goldDigitMaterial);
            widget->SetWorldMatrix(BuildCameraAnchoredUiMatrix(
                XMFLOAT2{ starCenter.x + offsets[piece].x, starCenter.y + offsets[piece].y },
                XMFLOAT2{ pieceSize, pieceSize },
                depth,
                0.018f));
            widget->Render(commandList);
        }
    }
}

bool TowerDefenseScene::WorldToScreenUi(const XMFLOAT3& worldPosition,
    XMFLOAT2& outNormalized,
    float& outDepth) const
{
    if (!m_camera || !g_framework) return false;

    const XMFLOAT3 delta = Utiles::Vector3::Sub(worldPosition, m_camera->GetEye());
    const float depth = Utiles::Vector3::Dot(delta, m_camera->GetN());
    if (depth <= 0.25f) return false;

    const float aspect = max(0.1f, g_framework->GetAspectRatio());
    const float halfViewHeight = depth * tanf(CameraFovY * 0.5f);
    const float halfViewWidth = halfViewHeight * aspect;
    if (halfViewHeight <= 0.001f || halfViewWidth <= 0.001f) return false;

    const float viewX = Utiles::Vector3::Dot(delta, m_camera->GetU());
    const float viewY = Utiles::Vector3::Dot(delta, m_camera->GetV());
    outNormalized.x = 0.5f + viewX / (halfViewWidth * 2.0f);
    outNormalized.y = 0.5f - viewY / (halfViewHeight * 2.0f);
    outDepth = depth;

    return outNormalized.x >= -0.08f && outNormalized.x <= 1.08f &&
        outNormalized.y >= -0.08f && outNormalized.y <= 1.08f;
}

void TowerDefenseScene::ToggleGameplayView()
{
    if (m_debugCameraEnabled) return;

    m_topDownView = !m_topDownView;
    m_rightMouseOrbiting = false;
    UpdateGameplayCamera(0.0f);
}

void TowerDefenseScene::MoveGameplayCamera(const XMFLOAT3& delta)
{
    m_cameraFocus = ClampCameraFocusToTerrain(Utiles::Vector3::Add(m_cameraFocus, delta));
}

void TowerDefenseScene::UpdateAngledMouseOrbit(HWND hWnd)
{
    POINT cursor{};
    float width = 0.0f;
    float height = 0.0f;
    if (!GetClientCursor(hWnd, cursor, width, height)) return;

    const float dx = static_cast<float>(cursor.x - m_lastOrbitCursor.x);
    const float dy = static_cast<float>(cursor.y - m_lastOrbitCursor.y);
    m_lastOrbitCursor = cursor;

    if (fabsf(dx) <= 0.01f && fabsf(dy) <= 0.01f) return;

    constexpr float YawSensitivity = 0.0062f;
    constexpr float PitchSensitivity = 0.0046f;
    m_cameraYaw += dx * YawSensitivity;
    m_cameraPitch = clamp(m_cameraPitch - dy * PitchSensitivity, MinOrbitPitch, MaxOrbitPitch);

    if (fabsf(m_cameraYaw) > XM_2PI) m_cameraYaw = fmodf(m_cameraYaw, XM_2PI);
    UpdateGameplayCamera(0.0f);
}

XMFLOAT3 TowerDefenseScene::ClampCameraFocusToTerrain(const XMFLOAT3& focus) const
{
    XMFLOAT3 clamped = focus;
    if (m_mode == TowerDefenseMode::Playing && m_terrainHeightMap)
    {
        const float halfWidth = m_terrainHeightMap->GetWorldWidth() * 0.5f;
        const float halfLength = m_terrainHeightMap->GetWorldLength() * 0.5f;
        const float margin = min(6.0f, max(1.0f, min(halfWidth, halfLength) - 1.0f));
        clamped.x = clamp(clamped.x, -halfWidth + margin, halfWidth - margin);
        clamped.z = clamp(clamped.z, -halfLength + margin, halfLength - margin);
        clamped.y = TerrainHeightAtWorldXZ(clamped.x, clamped.z);
    }
    else
    {
        clamped.y = 0.0f;
    }

    return clamped;
}

XMFLOAT3 TowerDefenseScene::ApplySpringCameraCollision(const XMFLOAT3& focus, const XMFLOAT3& desiredEye) const
{
    XMFLOAT3 correctedEye = desiredEye;
    if (!m_terrainCollider) return correctedEye;

    XMFLOAT3 arm = Utiles::Vector3::Sub(desiredEye, focus);
    const float armLengthSq = Utiles::Vector3::Dot(arm, arm);
    if (armLengthSq > Utiles::Physics::Epsilon)
    {
        const float armLength = sqrtf(armLengthSq);
        const XMFLOAT3 armDirection = Utiles::Vector3::Mul(arm, 1.0f / armLength);
        float hitDistance = 0.0f;
        if (m_terrainCollider->Raycast(focus, armDirection, hitDistance) &&
            hitDistance > 0.65f && hitDistance < armLength - 0.85f)
        {
            correctedEye = Utiles::Vector3::Add(focus,
                Utiles::Vector3::Mul(armDirection, max(0.65f, hitDistance - 0.75f)));
        }
    }

    float terrainHeight = 0.0f;
    XMFLOAT3 terrainNormal{};
    if (m_terrainCollider->GetHeightAtWorld(correctedEye, terrainHeight, terrainNormal))
    {
        correctedEye.y = max(correctedEye.y, terrainHeight + 2.2f);
    }

    return correctedEye;
}

void TowerDefenseScene::RemoveRenderObject(const shared_ptr<GameObject>& object)
{
    erase_if(m_objects, [&object](const shared_ptr<GameObject>& renderObject)
        {
            return renderObject == object;
        });
}

void TowerDefenseScene::RemoveSimulationObject(const shared_ptr<GameObject>& object)
{
    if (!object) return;

    if (m_collisionManager && object->GetCollider())
    {
        m_collisionManager->RemoveCollider(object->GetCollider());
    }

    if (m_physicsManager && object->GetRigidbody())
    {
        m_physicsManager->RemoveRigidbody(object->GetRigidbody());
    }

    RemoveRenderObject(object);
}
