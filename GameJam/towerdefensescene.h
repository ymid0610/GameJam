#pragma once
#include "scene.h"
#include "terrain.h"

enum class TowerDefenseMode
{
    StartScreen,
    Playing,
    Victory,
    Defeat
};

enum class TowerDefenseTowerType
{
    Basic,
    Rapid,
    Splash,
    Slow
};

struct TowerDefenseOffer
{
    TowerDefenseTowerType type = TowerDefenseTowerType::Basic;
    int tier = 1;
    int cost = 1;
};

struct TowerDefenseEnemy
{
    shared_ptr<GameObject> object;
    shared_ptr<GameObject> healthBarBack;
    shared_ptr<GameObject> healthBarFill;
    size_t waypointIndex = 0;
    size_t routeIndex = 0;
    float health = 1.0f;
    float maxHealth = 1.0f;
    float speed = 1.0f;
    float heightOffset = 0.0f;
    float slowTimer = 0.0f;
    float slowMultiplier = 1.0f;
};

struct TowerDefenseTower
{
    shared_ptr<GameObject> object;
    XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    TowerDefenseTowerType type = TowerDefenseTowerType::Basic;
    int tier = 1;
    float range = 3.0f;
    float damage = 20.0f;
    float fireInterval = 0.55f;
    float splashRadius = 0.0f;
    float slowDuration = 0.0f;
    float slowMultiplier = 1.0f;
    float cooldown = 0.0f;
    weak_ptr<GameObject> target;
};

struct TowerDefenseHitMarker
{
    shared_ptr<GameObject> object;
    XMFLOAT3 velocity{ 0.0f, 0.0f, 0.0f };
    float age = 0.0f;
    float lifetime = 0.0f;
    float gravity = 0.0f;
};

struct TowerDefenseScopeMarker
{
    vector<shared_ptr<GameObject>> objects;
    weak_ptr<GameObject> target;
    float lifetime = 0.0f;
    float radius = 0.4f;
};

struct TowerDefenseProjectile
{
    shared_ptr<GameObject> object;
    weak_ptr<GameObject> target;
    XMFLOAT3 start{ 0.0f, 0.0f, 0.0f };
    TowerDefenseTowerType type = TowerDefenseTowerType::Basic;
    float damage = 0.0f;
    float splashRadius = 0.0f;
    float slowDuration = 0.0f;
    float slowMultiplier = 1.0f;
    float elapsed = 0.0f;
    float duration = 0.18f;
};

struct TowerDefenseTextRun
{
    XMFLOAT2 center{ 0.0f, 0.0f };
    XMFLOAT2 size{ 0.0f, 0.0f };
};

struct TowerDefenseTextCache
{
    wstring text;
    int pixelHeight = 0;
    float width = 1.0f;
    float height = 1.0f;
    vector<TowerDefenseTextRun> runs;
};

class TowerDefenseScene final : public Scene
{
public:
    TowerDefenseScene() = default;
    ~TowerDefenseScene() override = default;

    void Update(FLOAT timeElapsed) override;
    void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const override;
    void BuildObjects(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const ComPtr<ID3D12RootSignature>& rootSignature) override;
    void ReleaseObjects() override;
    void ReleaseUploadBuffer() override;
    void MouseEvent(HWND hWnd, FLOAT timeElapsed) override;
    void MouseButtonEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;
    void KeyboardEvent(FLOAT timeElapsed) override;
    bool OnKeyDown(WPARAM wParam) override;
    void MouseWheelEvent(WPARAM wParam) override;

private:
    void BuildMaterials(const ComPtr<ID3D12Device>& device);
    void BuildStartScreen();
    void StartGame();
    void BuildField();
    void BuildShop();
    void BuildTowerInfoUI();
    void BuildGoldUI();
    void BuildBitmapTextPool();
    void LoadBitmapFont();
    void ReleaseBitmapFont();
    void RollShopOffers(bool freeReroll);
    TowerDefenseOffer CreateRandomOffer() const;
    void BuildSun();
    void BuildMoon();
    void BuildTunnelMouth(const string& name, const XMFLOAT3& center, float directionSign);
    void BuildPath();
    void RenderShopUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderSelectedTowerRange(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderTowerInfoUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderStartScreenUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderGoldUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void BeginBitmapTextPass() const;
    void RenderBitmapText(const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const wstring& text,
        const XMFLOAT2& anchor,
        float normalizedHeight,
        float depth,
        const shared_ptr<Material>& material,
        float alignX = 0.0f) const;
    void RenderResultUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void UpdateGameplayCamera(float timeElapsed);
    void UpdateCelestialCycle(float timeElapsed);
    void UpdateEnemies(float timeElapsed);
    void UpdateTowers(float timeElapsed);
    void UpdateProjectiles(float timeElapsed);
    void UpdateHitMarkers(float timeElapsed);
    void UpdateScopeMarkers(float timeElapsed);
    void UpdateDragPreview(HWND hWnd);
    void UpdateEnemyHealthBars();
    void SpawnEnemy(float healthMultiplier = 1.0f,
        float speedMultiplier = 1.0f,
        float visualScale = 1.0f,
        float laneOffset = 0.0f);
    void SpawnHitMarker(const XMFLOAT3& position);
    void SpawnDeathEffect(const XMFLOAT3& position, float scale);
    void SpawnCoinDropEffect(const XMFLOAT3& position, int amount);
    void SpawnParticle(const XMFLOAT3& position,
        const XMFLOAT3& velocity,
        const XMFLOAT3& scale,
        const shared_ptr<Material>& material,
        float lifetime,
        float gravity = 0.0f);
    void SpawnScopeMarker(const shared_ptr<GameObject>& target, float duration, float radius);
    void SpawnProjectile(const TowerDefenseTower& tower, const shared_ptr<GameObject>& target);
    bool TryPlaceTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryMergeSelectedTowerWith(const shared_ptr<GameObject>& targetObject);
    void ApplyTowerStats(TowerDefenseTower& tower);
    void UpdateTowerVisual(TowerDefenseTower& tower);
    void HandleClick(HWND hWnd);
    void BeginTowerDrag(int offerSlot);
    void EndTowerDrag(HWND hWnd);
    void ClearDragGhost();
    void EnableShadowCasting(const shared_ptr<GameObject>& object) const;
    void RemoveRenderObject(const shared_ptr<GameObject>& object);
    void RemoveSimulationObject(const shared_ptr<GameObject>& object);

    shared_ptr<GameObject> CreateCubeObject(const string& name,
        const XMFLOAT3& position,
        const XMFLOAT3& scale,
        const shared_ptr<Material>& material) const;
    shared_ptr<GameObject> CreateCapsuleObject(const string& name,
        const XMFLOAT3& position,
        const shared_ptr<Material>& material,
        float scale = 1.0f) const;

    bool TryGetGroundPoint(HWND hWnd, XMFLOAT3& outPoint) const;
    bool TryGetPointOnPlane(HWND hWnd, float planeY, XMFLOAT3& outPoint) const;
    bool TryGetTerrainPoint(HWND hWnd, XMFLOAT3& outPoint) const;
    bool TryGetShopSlotFromCursor(HWND hWnd, int& outSlot) const;
    bool CanPlaceTower(const XMFLOAT3& terrainPoint) const;
    bool IsNearPath(const XMFLOAT3& terrainPoint, float radius) const;
    bool HasTowerNear(const XMFLOAT3& terrainPoint, float radius) const;
    bool IsEnemyInTowerRange(const TowerDefenseTower& tower, const TowerDefenseEnemy& enemy) const;
    TowerDefenseTower* FindTowerAtPoint(const XMFLOAT3& terrainPoint);
    TowerDefenseEnemy* FindEnemyByObject(const shared_ptr<GameObject>& object);
    TowerDefenseEnemy* AcquireTowerTarget(const TowerDefenseTower& tower);
    const TowerDefenseTower* GetSelectedTower() const;
    int GetRerollCost() const;
    const TowerDefenseTextCache& GetBitmapTextCache(const wstring& text, int pixelHeight) const;
    TowerDefenseTextCache RasterizeBitmapText(const wstring& text, int pixelHeight) const;
    wstring ResolveBitmapFontPath() const;
    XMFLOAT3 TerrainWorldPosition(float localX, float localZ, float yOffset = 0.0f) const;
    float TerrainHeightAtWorldXZ(float x, float z) const;
    void PositionScopeMarker(TowerDefenseScopeMarker& marker) const;
    bool GetShopSlotRect(int tier, float width, float height, XMFLOAT2& outCenter, XMFLOAT2& outHalfSize) const;
    bool IsTowerInfoPanelUnderCursor(HWND hWnd) const;
    XMFLOAT4X4 BuildCameraAnchoredUiMatrix(const XMFLOAT2& normalizedCenter,
        const XMFLOAT2& normalizedSize,
        float depth,
        float thickness) const;
    void ToggleGameplayView();
    void MoveGameplayCamera(const XMFLOAT3& delta);
    void UpdateAngledMouseOrbit(HWND hWnd);
    XMFLOAT3 ClampCameraFocusToTerrain(const XMFLOAT3& focus) const;
    XMFLOAT3 ApplySpringCameraCollision(const XMFLOAT3& focus, const XMFLOAT3& desiredEye) const;

private:
    static constexpr float CameraFovY = XM_PIDIV4;
    static constexpr float DefaultOrbitPitch = XM_PI / 3.0f;
    static constexpr float MinOrbitPitch = XM_PI / 6.0f;
    static constexpr float MaxOrbitPitch = XM_PI * 7.0f / 18.0f;
    static constexpr float ShopZ = -18.5f;
    static constexpr UINT TerrainSamples = 221;
    static constexpr float TerrainCellSpacing = 0.38f;
    static constexpr float TowerSpacing = 1.15f;
    static constexpr float PathBlockRadius = 1.35f;
    static constexpr float TowerPickRadius = 0.95f;
    static constexpr float EnemyCapsuleRadius = 0.20f;
    static constexpr float EnemyCapsuleBodyHeight = 0.38f;
    static constexpr float EnemyHalfHeight = EnemyCapsuleRadius + EnemyCapsuleBodyHeight * 0.5f;
    static constexpr float TowerHalfHeight = 0.60f;
    static constexpr int TowerTypeCount = 4;
    static constexpr int MaxTowerTier = 3;
    static constexpr int MaxWave = 6;

    TowerDefenseMode m_mode = TowerDefenseMode::StartScreen;
    vector<XMFLOAT3> m_waypoints;
    vector<vector<XMFLOAT3>> m_enemyPaths;
    vector<TowerDefenseTower> m_towers;
    vector<TowerDefenseEnemy> m_enemies;
    vector<TowerDefenseHitMarker> m_hitMarkers;
    vector<TowerDefenseScopeMarker> m_scopeMarkers;
    vector<TowerDefenseProjectile> m_projectiles;

    shared_ptr<Material> m_startMaterial;
    shared_ptr<Material> m_startAccentMaterial;
    shared_ptr<Material> m_fieldMaterial;
    shared_ptr<Material> m_blockedMaterial;
    shared_ptr<Material> m_shopPanelMaterial;
    shared_ptr<Material> m_shopSlotMaterial;
    shared_ptr<Material> m_infoBarBackgroundMaterial;
    shared_ptr<Material> m_infoDamageMaterial;
    shared_ptr<Material> m_infoRangeMaterial;
    shared_ptr<Material> m_infoFireRateMaterial;
    shared_ptr<Material> m_infoCooldownMaterial;
    shared_ptr<Material> m_healthBarBackMaterial;
    shared_ptr<Material> m_healthBarFillMaterial;
    shared_ptr<Material> m_deathEffectMaterial;
    shared_ptr<Material> m_coinMaterial;
    shared_ptr<Material> m_goldUiMaterial;
    shared_ptr<Material> m_goldDigitMaterial;
    shared_ptr<Material> m_bitmapTextMaterial;
    shared_ptr<Material> m_resultVictoryMaterial;
    shared_ptr<Material> m_resultDefeatMaterial;
    shared_ptr<Material> m_shopTowerMaterials[TowerTypeCount][MaxTowerTier];
    shared_ptr<Material> m_towerMaterials[TowerTypeCount][MaxTowerTier];
    shared_ptr<Material> m_dragGhostMaterial;
    shared_ptr<Material> m_enemyMaterial;
    shared_ptr<Material> m_hitMaterial;
    shared_ptr<Material> m_scopeMaterial;
    shared_ptr<Material> m_projectileMaterial;
    shared_ptr<Material> m_tunnelOpeningMaterial;
    shared_ptr<Material> m_tunnelStoneMaterial;
    shared_ptr<Material> m_sunMaterial;
    shared_ptr<Material> m_moonMaterial;

    shared_ptr<TerrainHeightMap> m_terrainHeightMap;
    shared_ptr<TerrainCollider> m_terrainCollider;
    shared_ptr<GameObject> m_terrainObject;
    shared_ptr<GameObject> m_shopPanel;
    vector<shared_ptr<GameObject>> m_shopSlots;
    vector<shared_ptr<GameObject>> m_towerInfoWidgets;
    vector<shared_ptr<GameObject>> m_goldUiWidgets;
    vector<shared_ptr<GameObject>> m_selectedTowerRangeMarkers;
    mutable vector<shared_ptr<GameObject>> m_bitmapTextRects;
    mutable vector<TowerDefenseTextCache> m_bitmapTextCache;
    mutable size_t m_bitmapTextCursor = 0;
    shared_ptr<GameObject> m_sunObject;
    shared_ptr<PointLight> m_sunLight;
    shared_ptr<GameObject> m_moonObject;
    shared_ptr<PointLight> m_moonLight;

    bool m_rightMouseOrbiting = false;
    bool m_draggingTower = false;
    bool m_topDownView = false;
    int m_dragTier = 1;
    POINT m_lastOrbitCursor{};
    XMFLOAT3 m_cameraFocus{ 0.0f, 0.0f, 0.0f };
    float m_cameraZoom = 42.0f;
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = DefaultOrbitPitch;
    float m_sunCycleTime = 0.0f;
    bool m_bitmapFontLoaded = false;
    wstring m_bitmapFontPath;
    wstring m_bitmapFontFamily = L"Terrarum Sans Bitmap";
    weak_ptr<GameObject> m_selectedTower;
    TowerDefenseOffer m_shopOffers[3];
    TowerDefenseOffer m_dragOffer;
    shared_ptr<GameObject> m_dragGhost;
    float m_spawnTimer = 0.0f;
    int m_wave = 1;
    int m_spawnedInWave = 0;
    int m_lives = 20;
    int m_gold = 10;
};
