#include "towerdefensescene.h"
#include "framework.h"
#include "towerdefenseroute.h"

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
        Coin
    };

    size_t WidgetIndex(TowerInfoWidget widget)
    {
        return static_cast<size_t>(widget);
    }

    size_t GoldWidgetIndex(GoldUiWidget widget)
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
        default:
            return 0;
        }
    }

    TowerDefenseTowerType TowerTypeFromIndex(int index)
    {
        switch (clamp(index, 0, 3))
        {
        case 1:
            return TowerDefenseTowerType::Rapid;
        case 2:
            return TowerDefenseTowerType::Splash;
        case 3:
            return TowerDefenseTowerType::Slow;
        default:
            return TowerDefenseTowerType::Basic;
        }
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
        const float tierBoost = static_cast<float>(tier - 1) * 0.11f;
        XMFLOAT3 color{};
        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            color = XMFLOAT3{ 1.0f, 0.72f, 0.22f };
            break;
        case TowerDefenseTowerType::Splash:
            color = XMFLOAT3{ 1.0f, 0.32f, 0.24f };
            break;
        case TowerDefenseTowerType::Slow:
            color = XMFLOAT3{ 0.34f, 0.95f, 1.0f };
            break;
        default:
            color = XMFLOAT3{ 0.45f, 0.76f, 1.0f };
            break;
        }

        return XMFLOAT4{
            clamp(color.x + tierBoost, 0.0f, 1.0f),
            clamp(color.y + tierBoost, 0.0f, 1.0f),
            clamp(color.z + tierBoost, 0.0f, 1.0f),
            alpha
        };
    }

    struct TowerStats
    {
        float range = 3.0f;
        float damage = 20.0f;
        float fireInterval = 0.55f;
        float splashRadius = 0.0f;
        float slowDuration = 0.0f;
        float slowMultiplier = 1.0f;
    };

    TowerStats BuildTowerStats(TowerDefenseTowerType type, int tier)
    {
        tier = clamp(tier, 1, 3);
        const float t = static_cast<float>(tier);

        switch (type)
        {
        case TowerDefenseTowerType::Rapid:
            return TowerStats{ 2.55f + t * 0.18f, 6.5f + t * 4.0f, max(0.19f, 0.32f - t * 0.025f) };
        case TowerDefenseTowerType::Splash:
            return TowerStats{ 2.65f + t * 0.26f, 11.0f + t * 7.5f, max(0.55f, 0.92f - t * 0.085f), 0.82f + t * 0.22f };
        case TowerDefenseTowerType::Slow:
            return TowerStats{ 3.05f + t * 0.24f, 7.0f + t * 4.5f, max(0.42f, 0.76f - t * 0.045f), 0.0f, 1.25f + t * 0.22f, max(0.38f, 0.68f - t * 0.07f) };
        default:
            return TowerStats{ 2.55f + t * 0.35f, 16.0f + t * 8.0f, max(0.40f, 0.62f - t * 0.06f) };
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
    m_terrainHeightMap = TerrainHeightMap::CreateWaveField(TerrainSamples, TerrainSamples,
        TerrainCellSpacing, 2.25f, 2.50f);
    m_terrainMesh = make_shared<TerrainMesh>(device, commandList, m_terrainHeightMap);

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
    m_shopPanelMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.06f, 0.075f, 0.09f, 0.84f });
    m_shopPanelMaterial->SetEmission(XMFLOAT3{ 0.02f, 0.08f, 0.12f }, 0.30f);
    m_shopSlotMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.16f, 0.18f, 0.20f, 0.88f });
    m_shopSlotMaterial->SetEmission(XMFLOAT3{ 0.04f, 0.09f, 0.12f }, 0.38f);
    m_infoBarBackgroundMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.08f, 0.095f, 0.11f, 0.92f });
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
    m_goldDigitMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.94f, 0.38f, 0.98f });
    m_goldDigitMaterial->SetEmission(XMFLOAT3{ 0.82f, 0.58f, 0.12f }, 0.60f);
    m_bitmapTextMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.96f, 0.72f, 0.98f });
    m_bitmapTextMaterial->SetEmission(XMFLOAT3{ 0.85f, 0.62f, 0.20f }, 0.62f);
    m_resultVictoryMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 0.18f, 0.95f, 0.38f, 0.88f });
    m_resultVictoryMaterial->SetEmission(XMFLOAT3{ 0.08f, 0.70f, 0.20f }, 0.70f);
    m_resultDefeatMaterial = Material::Create(device, m_overlayShader, XMFLOAT4{ 1.0f, 0.22f, 0.18f, 0.88f });
    m_resultDefeatMaterial->SetEmission(XMFLOAT3{ 0.80f, 0.05f, 0.04f }, 0.70f);

    m_dragGhostMaterial = Material::Create(device, m_shader, TowerColor(TowerDefenseTowerType::Basic, 1, 0.38f));
    m_dragGhostMaterial->SetEmission(XMFLOAT3{ 0.30f, 0.55f, 1.0f }, 0.35f);

    m_enemyMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.20f, 0.18f, 1.0f });
    m_hitMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.95f, 0.25f, 1.0f });
    m_hitMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.85f, 0.15f }, 0.75f);
    m_scopeMaterial = Material::Create(device, m_shader, XMFLOAT4{ 0.18f, 0.92f, 1.0f, 0.74f });
    m_scopeMaterial->SetEmission(XMFLOAT3{ 0.12f, 0.88f, 1.0f }, 1.45f);
    m_projectileMaterial = Material::Create(device, m_shader, XMFLOAT4{ 1.0f, 0.86f, 0.18f, 1.0f });
    m_projectileMaterial->SetEmission(XMFLOAT3{ 1.0f, 0.62f, 0.08f }, 2.75f);
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
    constexpr int BranchWaypointCount = 16;
    constexpr int SharedWaypointCount = 16;

    for (int route = 0; route < TowerDefenseRoute::RouteCount; ++route)
    {
        vector<XMFLOAT3> path;
        path.reserve(BranchWaypointCount + SharedWaypointCount);

        for (int i = 0; i < BranchWaypointCount; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(BranchWaypointCount - 1);
            float normalizedX = TowerDefenseRoute::StartX + t * (TowerDefenseRoute::MeetX - TowerDefenseRoute::StartX);
            float normalizedZ = clamp(TowerDefenseRoute::CenterZ(route, normalizedX), 0.08f, 0.92f);
            path.push_back(TerrainWorldPosition(width * normalizedX, length * normalizedZ, EnemyHalfHeight));
        }

        for (int i = 1; i < SharedWaypointCount; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(SharedWaypointCount - 1);
            float normalizedX = TowerDefenseRoute::MeetX + t * (TowerDefenseRoute::EndX - TowerDefenseRoute::MeetX);
            float normalizedZ = clamp(TowerDefenseRoute::CenterZ(route, normalizedX), 0.08f, 0.92f);
            path.push_back(TerrainWorldPosition(width * normalizedX, length * normalizedZ, EnemyHalfHeight));
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
    m_enemies.clear();
    m_hitMarkers.clear();
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
    m_selectedTowerRangeMarkers.clear();
    m_bitmapTextCache.clear();
    m_sunObject.reset();
    m_moonObject.reset();
    m_rightMouseOrbiting = false;
    m_topDownView = false;
    m_cameraYaw = 0.0f;
    m_cameraPitch = DefaultOrbitPitch;
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
}

void TowerDefenseScene::StartGame()
{
    m_mode = TowerDefenseMode::Playing;
    m_objects.clear();
    m_towers.clear();
    m_enemies.clear();
    m_hitMarkers.clear();
    m_scopeMarkers.clear();
    m_projectiles.clear();
    ClearDragGhost();
    m_selectedTower.reset();
    m_enemyPaths.clear();
    m_shopPanel.reset();
    m_shopSlots.clear();
    m_towerInfoWidgets.clear();
    m_goldUiWidgets.clear();
    m_selectedTowerRangeMarkers.clear();
    m_bitmapTextCache.clear();
    m_sunObject.reset();
    m_moonObject.reset();
    m_rightMouseOrbiting = false;
    m_topDownView = false;
    m_cameraYaw = 0.0f;
    m_cameraPitch = DefaultOrbitPitch;
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
    m_lives = 20;
    m_gold = 10;
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

    BuildTowerInfoUI();
    BuildGoldUI();
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
    for (int i = 0; i < 4; ++i)
    {
        m_selectedTowerRangeMarkers.push_back(CreateCubeObject("Selected Tower Range",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_scopeMaterial));
    }
}

void TowerDefenseScene::BuildGoldUI()
{
    constexpr size_t GoldWidgetCount = 2u;
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
}

void TowerDefenseScene::BuildBitmapTextPool()
{
    m_bitmapTextRects.clear();
    m_bitmapTextRects.reserve(1600);
    for (int i = 0; i < 1600; ++i)
    {
        m_bitmapTextRects.push_back(CreateCubeObject("Bitmap Text Rect",
            XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
            XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            m_bitmapTextMaterial));
    }
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

    const int typeIndex = Utiles::Random::GetInt(0, TowerTypeCount - 1);
    TowerDefenseOffer offer{};
    offer.type = TowerTypeFromIndex(typeIndex);
    offer.tier = tier;
    offer.cost = 2 + tier * 2;
    if (offer.type == TowerDefenseTowerType::Splash) ++offer.cost;
    if (offer.type == TowerDefenseTowerType::Slow) ++offer.cost;
    return offer;
}

void TowerDefenseScene::RenderShopUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::Playing || !m_camera || !g_framework) return;

    const float width = static_cast<float>(g_framework->GetWindowWidth());
    const float height = static_cast<float>(g_framework->GetWindowHeight());
    if (width <= 1.0f || height <= 1.0f) return;

    if (m_shopPanel)
    {
        XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(
            XMFLOAT2{ 0.5f, 0.865f },
            XMFLOAT2{ 0.38f, 0.16f },
            4.2f,
            0.025f);
        m_shopPanel->SetWorldMatrix(world);
        m_shopPanel->Render(commandList);
    }

    size_t objectIndex = 0;
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
            XMFLOAT4X4 slotWorld = BuildCameraAnchoredUiMatrix(
                normalizedCenter,
                normalizedSlotSize,
                4.05f,
                0.030f);
            m_shopSlots[objectIndex]->SetWorldMatrix(slotWorld);
            m_shopSlots[objectIndex]->Render(commandList);
        }
        ++objectIndex;

        if (objectIndex < m_shopSlots.size() && m_shopSlots[objectIndex])
        {
            const TowerDefenseOffer& offer = m_shopOffers[slot - 1];
            const int offerType = TowerTypeIndex(offer.type);
            const int offerTier = clamp(offer.tier, 1, MaxTowerTier);
            const float iconScale = 0.52f + static_cast<float>(offerTier) * 0.06f;
            XMFLOAT4X4 iconWorld = BuildCameraAnchoredUiMatrix(
                normalizedCenter,
                XMFLOAT2{ normalizedSlotSize.x * iconScale, normalizedSlotSize.y * iconScale },
                3.92f,
                0.12f);
            m_shopSlots[objectIndex]->SetMaterial(m_shopTowerMaterials[offerType][offerTier - 1]);
            m_shopSlots[objectIndex]->SetWorldMatrix(iconWorld);
            m_shopSlots[objectIndex]->Render(commandList);

            WCHAR costText[16]{};
            swprintf_s(costText, L"%dC", offer.cost);
            RenderBitmapText(commandList,
                costText,
                XMFLOAT2{ normalizedCenter.x, normalizedCenter.y + normalizedSlotSize.y * 0.25f },
                0.025f,
                3.72f,
                m_goldDigitMaterial,
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
}

void TowerDefenseScene::RenderSelectedTowerRange(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    const TowerDefenseTower* tower = GetSelectedTower();
    if (!tower || !tower->object || m_selectedTowerRangeMarkers.size() < 4) return;

    const XMFLOAT3 position = tower->object->GetPosition();
    const float range = max(0.25f, tower->range);
    const float y = TerrainHeightAtWorldXZ(position.x, position.z) + 0.055f;
    constexpr float Thickness = 0.035f;

    const XMFLOAT3 centers[4] = {
        XMFLOAT3{ position.x - range, y, position.z },
        XMFLOAT3{ position.x + range, y, position.z },
        XMFLOAT3{ position.x, y, position.z - range },
        XMFLOAT3{ position.x, y, position.z + range }
    };
    const XMFLOAT3 scales[4] = {
        XMFLOAT3{ Thickness, Thickness, range },
        XMFLOAT3{ Thickness, Thickness, range },
        XMFLOAT3{ range, Thickness, Thickness },
        XMFLOAT3{ range, Thickness, Thickness }
    };

    for (size_t i = 0; i < 4; ++i)
    {
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixScaling(scales[i].x, scales[i].y, scales[i].z) *
            XMMatrixTranslation(centers[i].x, centers[i].y, centers[i].z));
        m_selectedTowerRangeMarkers[i]->SetWorldMatrix(world);
        m_selectedTowerRangeMarkers[i]->Render(commandList);
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
    renderWidget(TowerInfoWidget::TowerIcon,
        XMFLOAT2{ 0.728f, 0.075f },
        XMFLOAT2{ iconScale, iconScale * 1.18f },
        3.92f,
        0.12f,
        m_shopTowerMaterials[typeIndex][tier - 1]);

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
        tower->damage / 56.0f, m_infoDamageMaterial);
    renderBar(1, TowerInfoWidget::RangeIcon, TowerInfoWidget::RangeTrack, TowerInfoWidget::RangeFill,
        tower->range / 4.20f, m_infoRangeMaterial);
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

    for (const auto& run : cache.runs)
    {
        if (m_bitmapTextCursor >= m_bitmapTextRects.size()) return;

        auto& rect = m_bitmapTextRects[m_bitmapTextCursor++];
        if (!rect) continue;

        if (material) rect->SetMaterial(material);
        const XMFLOAT2 center{
            startX + run.center.x * unitX,
            startY + run.center.y * unitY
        };
        const XMFLOAT2 size{
            max(unitX, run.size.x * unitX),
            max(unitY, run.size.y * unitY)
        };

        XMFLOAT4X4 world = BuildCameraAnchoredUiMatrix(center, size, depth, 0.010f);
        rect->SetWorldMatrix(world);
        rect->Render(commandList);
    }
}

void TowerDefenseScene::RenderStartScreenUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode != TowerDefenseMode::StartScreen) return;

    RenderBitmapText(commandList,
        L"START",
        XMFLOAT2{ 0.5f, 0.452f },
        0.065f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);
}

void TowerDefenseScene::RenderGoldUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_mode == TowerDefenseMode::StartScreen || !m_camera || !g_framework) return;
    if (m_goldUiWidgets.size() < 2u) return;

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
        XMFLOAT2{ 0.125f, 0.083f },
        XMFLOAT2{ 0.210f, 0.115f },
        4.18f,
        0.025f,
        m_shopPanelMaterial);

    renderWidget(GoldWidgetIndex(GoldUiWidget::Coin),
        XMFLOAT2{ 0.050f, 0.083f },
        XMFLOAT2{ 0.035f, 0.035f },
        3.92f,
        0.070f,
        m_goldUiMaterial);

    WCHAR goldText[32]{};
    swprintf_s(goldText, L"%d", max(0, m_gold));
    RenderBitmapText(commandList, goldText, XMFLOAT2{ 0.085f, 0.055f }, 0.062f, 3.78f, m_goldDigitMaterial);
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
        XMFLOAT2{ 0.42f, 0.18f },
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
        XMFLOAT2{ 0.5f, 0.408f },
        0.075f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);
    RenderBitmapText(commandList,
        L"ESC MENU",
        XMFLOAT2{ 0.5f, 0.505f },
        0.035f,
        3.70f,
        m_bitmapTextMaterial,
        0.5f);
}

void TowerDefenseScene::UpdateGameplayCamera(float timeElapsed)
{
    (void)timeElapsed;
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
    UpdateCelestialCycle(timeElapsed);
    UpdateGameplayCamera(timeElapsed);

    if (m_mode != TowerDefenseMode::Playing)
    {
        UpdateHitMarkers(timeElapsed);
        UpdateScopeMarkers(timeElapsed);
        return;
    }

    UpdateEnemies(timeElapsed);
    UpdateTowers(timeElapsed);
    UpdateProjectiles(timeElapsed);
    UpdateHitMarkers(timeElapsed);
    UpdateScopeMarkers(timeElapsed);

    if (m_lives <= 0)
    {
        m_mode = TowerDefenseMode::Defeat;
        ClearDragGhost();
        return;
    }

    const int waveSize = 10 + m_wave * 3;
    if (m_spawnedInWave < waveSize)
    {
        m_spawnTimer -= timeElapsed;
        if (m_spawnTimer <= 0.0f)
        {
            const int remaining = waveSize - m_spawnedInWave;
            int batchCount = 1;
            const float burstRoll = Utiles::Random::GetFloat(0.0f, 1.0f);
            if (m_wave >= 2 && burstRoll < 0.16f) batchCount = min(4, remaining);
            else if (burstRoll < 0.42f) batchCount = min(2, remaining);

            for (int i = 0; i < batchCount && m_spawnedInWave < waveSize; ++i)
            {
                const float largeChance = min(0.10f + static_cast<float>(m_wave) * 0.018f, 0.26f);
                const bool isLarge = m_wave >= 2 && Utiles::Random::GetFloat(0.0f, 1.0f) < largeChance;
                const float laneOffset = batchCount > 1
                    ? (static_cast<float>(i) - (static_cast<float>(batchCount) - 1.0f) * 0.5f) * 0.46f
                    : 0.0f;

                if (isLarge)
                {
                    SpawnEnemy(2.8f, 0.70f, 1.65f, laneOffset);
                }
                else
                {
                    SpawnEnemy(1.0f, Utiles::Random::GetFloat(0.94f, 1.08f), 1.0f, laneOffset);
                }

                ++m_spawnedInWave;
            }

            const float minInterval = max(0.30f, 0.92f - static_cast<float>(m_wave) * 0.035f);
            const float maxInterval = max(0.52f, 1.26f - static_cast<float>(m_wave) * 0.030f);
            m_spawnTimer = Utiles::Random::GetFloat(minInterval, maxInterval);
            if (batchCount > 1) m_spawnTimer *= 1.22f;
        }
    }
    else if (m_enemies.empty())
    {
        if (m_wave >= MaxWave)
        {
            m_mode = TowerDefenseMode::Victory;
            m_gold += 10;
            ClearDragGhost();
            return;
        }

        ++m_wave;
        m_spawnedInWave = 0;
        m_spawnTimer = 1.2f;
        m_gold += 3;
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
                const int coinReward = 1 + (it->maxHealth > 90.0f ? 1 : 0);
                m_gold += coinReward;
                if (it->object)
                {
                    const XMFLOAT3 deathPosition = it->object->GetPosition();
                    SpawnDeathEffect(deathPosition, max(1.0f, it->heightOffset / EnemyHalfHeight));
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
}

void TowerDefenseScene::UpdateEnemyHealthBars()
{
    for (auto& enemy : m_enemies)
    {
        if (!enemy.object || !enemy.healthBarBack || !enemy.healthBarFill) continue;

        const XMFLOAT3 position = enemy.object->GetPosition();
        const float visualScale = max(0.85f, enemy.heightOffset / EnemyHalfHeight);
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

        if (!target || tower.cooldown > 0.0f) continue;

        const float targetScale = max(1.0f, target->heightOffset / EnemyHalfHeight);
        SpawnProjectile(tower, target->object);
        SpawnScopeMarker(target->object, min(0.34f, tower.fireInterval * 0.72f), 0.42f * targetScale);
        tower.cooldown = tower.fireInterval;
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
                    enemy->health -= it->damage;
                    SpawnHitMarker(hitPosition);

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
                            if (DistanceSqXZ(splashEnemy.object->GetPosition(), hitPosition) > splashRadiusSq) continue;

                            splashEnemy.health -= it->damage * 0.58f;
                            SpawnHitMarker(splashEnemy.object->GetPosition());
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
    float laneOffset)
{
    if (m_enemyPaths.empty()) return;

    const size_t routeIndex = static_cast<size_t>(m_spawnedInWave) % m_enemyPaths.size();
    const auto& route = m_enemyPaths[routeIndex];
    if (route.empty()) return;

    visualScale = max(0.45f, visualScale);
    const float health = (42.0f + static_cast<float>(m_wave - 1) * 10.0f) * healthMultiplier;
    XMFLOAT3 spawn = route.front();
    if (route.size() > 1 && fabsf(laneOffset) > 0.001f)
    {
        XMFLOAT3 segment = Utiles::Vector3::Sub(route[1], route[0]);
        XMFLOAT3 side = Normalize(XMFLOAT3{ -segment.z, 0.0f, segment.x });
        spawn.x += side.x * laneOffset;
        spawn.z += side.z * laneOffset;
    }
    spawn.y = TerrainHeightAtWorldXZ(spawn.x, spawn.z) + EnemyHalfHeight * visualScale;
    auto enemyObject = CreateCapsuleObject("Enemy",
        spawn,
        m_enemyMaterial,
        visualScale);
    EnableShadowCasting(enemyObject);
    enemyObject->SetCollider(make_shared<CapsuleCollider>(
        EnemyCapsuleRadius * visualScale,
        EnemyCapsuleBodyHeight * visualScale));

    auto rigidbody = make_shared<Rigidbody>();
    rigidbody->SetUseGravity(false);
    rigidbody->SetDrag(0.0f);
    rigidbody->SetGroundFriction(0.0f);
    rigidbody->SetMass(1.0f);
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
        1,
        routeIndex,
        health,
        health,
        (1.45f + static_cast<float>(m_wave - 1) * 0.08f) * speedMultiplier,
        EnemyHalfHeight * visualScale,
        0.0f,
        1.0f
    });
}

void TowerDefenseScene::SpawnHitMarker(const XMFLOAT3& position)
{
    for (int i = 0; i < 3; ++i)
    {
        const float angle = Utiles::Random::GetFloat(0.0f, XM_2PI);
        const float speed = Utiles::Random::GetFloat(0.45f, 0.95f);
        SpawnParticle(
            XMFLOAT3{ position.x, position.y + 0.30f, position.z },
            XMFLOAT3{ cosf(angle) * speed, Utiles::Random::GetFloat(0.25f, 0.55f), sinf(angle) * speed },
            XMFLOAT3{ 0.07f, 0.07f, 0.07f },
            m_hitMaterial,
            0.20f,
            1.8f);
    }
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

    XMFLOAT3 start = tower.object->GetPosition();
    start.y += TowerHalfHeight * 0.95f;

    const float projectileSize = 0.18f + static_cast<float>(tower.tier) * 0.045f;
    auto projectileObject = CreateCubeObject("Tower Projectile",
        start,
        XMFLOAT3{ projectileSize, projectileSize, projectileSize },
        m_projectileMaterial);

    m_objects.push_back(projectileObject);
    m_projectiles.push_back(TowerDefenseProjectile{
        projectileObject,
        target,
        start,
        tower.type,
        tower.damage,
        tower.splashRadius,
        tower.slowDuration,
        tower.slowMultiplier,
        0.0f,
        0.24f
    });
}

void TowerDefenseScene::UpdateDragPreview(HWND hWnd)
{
    if (!m_draggingTower || !m_dragGhost) return;

    XMFLOAT3 point{};
    if (!TryGetTerrainPoint(hWnd, point)) return;

    bool validCell = CanPlaceTower(point);
    XMFLOAT3 position{ point.x, TerrainHeightAtWorldXZ(point.x, point.z) + TowerHalfHeight, point.z };

    XMFLOAT4 ghostColor = validCell
        ? TowerColor(m_dragOffer.type, m_dragOffer.tier, 0.42f)
        : XMFLOAT4{ 1.0f, 0.12f, 0.10f, 0.30f };
    m_dragGhostMaterial->SetBaseColor(ghostColor);

    XMFLOAT4X4 world{};
    const float tierScale = 1.0f + static_cast<float>(m_dragOffer.tier - 1) * 0.15f;
    XMStoreFloat4x4(&world, XMMatrixScaling(0.32f * tierScale, 0.60f * tierScale, 0.32f * tierScale) *
        XMMatrixTranslation(position.x, position.y, position.z));
    m_dragGhost->SetWorldMatrix(world);
}

bool TowerDefenseScene::TryPlaceTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer)
{
    if (!CanPlaceTower(terrainPoint) || m_gold < offer.cost) return false;

    const int tier = clamp(offer.tier, 1, MaxTowerTier);
    const int typeIndex = TowerTypeIndex(offer.type);
    XMFLOAT3 center{
        terrainPoint.x,
        TerrainHeightAtWorldXZ(terrainPoint.x, terrainPoint.z) + TowerHalfHeight,
        terrainPoint.z
    };
    const float tierScale = 1.0f + static_cast<float>(tier - 1) * 0.15f;
    auto towerObject = CreateCubeObject("Tower",
        center,
        XMFLOAT3{ 0.32f * tierScale, 0.60f * tierScale, 0.32f * tierScale },
        m_towerMaterials[typeIndex][tier - 1]);
    EnableShadowCasting(towerObject);
    towerObject->SetCollider(make_shared<BoxCollider>(m_cube));
    if (m_collisionManager) m_collisionManager->AddCollider(towerObject->GetCollider());

    m_objects.push_back(towerObject);
    TowerDefenseTower tower{};
    tower.object = towerObject;
    tower.position = center;
    tower.type = offer.type;
    tower.tier = tier;
    ApplyTowerStats(tower);
    m_towers.push_back(tower);
    m_selectedTower = towerObject;
    m_gold -= offer.cost;
    return true;
}

void TowerDefenseScene::ApplyTowerStats(TowerDefenseTower& tower)
{
    tower.tier = clamp(tower.tier, 1, MaxTowerTier);
    const TowerStats stats = BuildTowerStats(tower.type, tower.tier);
    tower.range = stats.range;
    tower.damage = stats.damage;
    tower.fireInterval = stats.fireInterval;
    tower.splashRadius = stats.splashRadius;
    tower.slowDuration = stats.slowDuration;
    tower.slowMultiplier = stats.slowMultiplier;
}

void TowerDefenseScene::UpdateTowerVisual(TowerDefenseTower& tower)
{
    if (!tower.object) return;

    tower.tier = clamp(tower.tier, 1, MaxTowerTier);
    const int typeIndex = TowerTypeIndex(tower.type);
    tower.object->SetMaterial(m_towerMaterials[typeIndex][tower.tier - 1]);

    const float tierScale = 1.0f + static_cast<float>(tower.tier - 1) * 0.15f;
    const XMFLOAT3 position = tower.object->GetPosition();
    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixScaling(0.32f * tierScale, 0.60f * tierScale, 0.32f * tierScale) *
        XMMatrixTranslation(position.x, position.y, position.z));
    tower.object->SetWorldMatrix(world);
    tower.position = position;
}

bool TowerDefenseScene::TryMergeSelectedTowerWith(const shared_ptr<GameObject>& targetObject)
{
    auto selectedObject = m_selectedTower.lock();
    if (!selectedObject || !targetObject || selectedObject == targetObject) return false;

    int selectedIndex = -1;
    int targetIndex = -1;
    for (int i = 0; i < static_cast<int>(m_towers.size()); ++i)
    {
        if (m_towers[i].object == selectedObject) selectedIndex = i;
        if (m_towers[i].object == targetObject) targetIndex = i;
    }

    if (selectedIndex < 0 || targetIndex < 0) return false;

    TowerDefenseTower& selected = m_towers[selectedIndex];
    TowerDefenseTower& target = m_towers[targetIndex];
    if (selected.type != target.type || selected.tier != target.tier || target.tier >= MaxTowerTier) return false;

    const shared_ptr<GameObject> survivorObject = target.object;
    const XMFLOAT3 survivorPosition = survivorObject ? survivorObject->GetPosition() : target.position;

    target.tier += 1;
    ApplyTowerStats(target);
    UpdateTowerVisual(target);
    target.cooldown = 0.0f;
    target.target.reset();

    SpawnDeathEffect(survivorPosition, 0.7f + static_cast<float>(target.tier) * 0.18f);
    RemoveSimulationObject(selected.object);

    m_towers.erase(m_towers.begin() + selectedIndex);
    m_selectedTower = survivorObject;
    return true;
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
        XMFLOAT3 point{};
        if (!TryGetGroundPoint(hWnd, point)) return;

        if (fabsf(point.x) <= 2.15f && fabsf(point.z) <= 1.05f)
        {
            StartGame();
        }
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
            if (!TryMergeSelectedTowerWith(tower->object))
            {
                m_selectedTower = tower->object;
            }
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

    m_draggingTower = true;
    m_dragTier = clamp(m_dragOffer.tier, 1, MaxTowerTier);
    m_dragGhostMaterial->SetBaseColor(TowerColor(m_dragOffer.type, m_dragOffer.tier, 0.38f));

    const float tierScale = 1.0f + static_cast<float>(m_dragTier - 1) * 0.15f;
    m_dragGhost = CreateCubeObject("Tower Drag Preview",
        XMFLOAT3{ 0.0f, -1000.0f, 0.0f },
        XMFLOAT3{ 0.32f * tierScale, 0.60f * tierScale, 0.32f * tierScale },
        m_dragGhostMaterial);
    m_objects.push_back(m_dragGhost);
}

void TowerDefenseScene::EndTowerDrag(HWND hWnd)
{
    XMFLOAT3 point{};
    if (TryGetTerrainPoint(hWnd, point))
    {
        (void)TryPlaceTower(point, m_dragOffer);
    }

    ClearDragGhost();
}

void TowerDefenseScene::ClearDragGhost()
{
    if (m_dragGhost)
    {
        RemoveRenderObject(m_dragGhost);
        m_dragGhost.reset();
    }

    m_draggingTower = false;
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

    if (wParam == 'R' && m_mode == TowerDefenseMode::Playing)
    {
        RollShopOffers(false);
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
    RenderShopUI(commandList);
    RenderGoldUI(commandList);
    RenderTowerInfoUI(commandList);
    RenderResultUI(commandList);
}

void TowerDefenseScene::ReleaseObjects()
{
    m_towers.clear();
    m_enemies.clear();
    m_hitMarkers.clear();
    m_scopeMarkers.clear();
    m_projectiles.clear();
    ClearDragGhost();
    m_selectedTower.reset();
    m_waypoints.clear();
    m_enemyPaths.clear();
    m_terrainHeightMap.reset();
    m_terrainCollider.reset();
    m_terrainObject.reset();
    m_shopPanel.reset();
    m_shopSlots.clear();
    m_towerInfoWidgets.clear();
    m_goldUiWidgets.clear();
    m_selectedTowerRangeMarkers.clear();
    m_bitmapTextRects.clear();
    m_bitmapTextCache.clear();
    m_sunObject.reset();
    m_moonObject.reset();
    m_sunLight.reset();
    m_moonLight.reset();
    m_rightMouseOrbiting = false;
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
    for (auto& materialRow : m_shopTowerMaterials)
    {
        for (auto& material : materialRow) material.reset();
    }
    for (auto& materialRow : m_towerMaterials)
    {
        for (auto& material : materialRow) material.reset();
    }
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
    if (m_mode != TowerDefenseMode::Playing) return false;

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
    return any_of(m_towers.begin(), m_towers.end(), [terrainPoint, radiusSq](const TowerDefenseTower& tower)
        {
            return DistanceSqXZ(tower.position, terrainPoint) <= radiusSq;
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

bool TowerDefenseScene::IsEnemyInTowerRange(const TowerDefenseTower& tower, const TowerDefenseEnemy& enemy) const
{
    if (!tower.object || !enemy.object || enemy.health <= 0.0f) return false;

    const float rangeSq = tower.range * tower.range;
    return DistanceSqXZ(tower.object->GetPosition(), enemy.object->GetPosition()) <= rangeSq;
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
    }

    return nullptr;
}

int TowerDefenseScene::GetRerollCost() const
{
    return 1 + max(0, m_wave - 1) / 2;
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
    float bestDistanceSq = tower.range * tower.range;
    const XMFLOAT3 towerPosition = tower.object ? tower.object->GetPosition() : tower.position;

    for (auto& enemy : m_enemies)
    {
        if (!enemy.object || enemy.health <= 0.0f) continue;

        float distanceSq = DistanceSqXZ(towerPosition, enemy.object->GetPosition());
        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
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
