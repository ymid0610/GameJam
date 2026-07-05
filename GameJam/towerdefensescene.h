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
    Slow,
    Mortar,
    Flak
};

enum class TowerDefenseOfferKind
{
    Tower,
    Meteor,
    Freeze,
    Boost,
    Generator,
    Boulder
};

enum class TowerDefenseEnemyVariant
{
    Walker,
    Runner,
    Brute,
    Armored,
    FlyingScout,
    FlyingSplitter,
    Boss
};

struct TowerDefenseOffer
{
    TowerDefenseOfferKind kind = TowerDefenseOfferKind::Tower;
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
    float visualScale = 1.0f;
    bool isBoss = false;
    bool isFlying = false;
    bool splitsOnDeath = false;
    int splitCount = 0;
    TowerDefenseEnemyVariant variant = TowerDefenseEnemyVariant::Walker;
};

struct TowerDefenseTowerPartInstance
{
    shared_ptr<GameObject> object;
    size_t assetIndex = 0;
    bool rotatesWithTarget = false;
};

struct TowerDefenseTower
{
    shared_ptr<GameObject> object;
    vector<TowerDefenseTowerPartInstance> parts;
    XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    TowerDefenseTowerType type = TowerDefenseTowerType::Basic;
    int tier = 1;
    float minRange = 0.0f;
    float range = 3.0f;
    float damage = 20.0f;
    float fireInterval = 0.55f;
    float splashRadius = 0.0f;
    float slowDuration = 0.0f;
    float slowMultiplier = 1.0f;
    float cooldown = 0.0f;
    float boostTimer = 0.0f;
    float boostDamageMultiplier = 1.0f;
    float boostFireRateMultiplier = 1.0f;
    bool targetsGround = true;
    bool targetsAir = true;
    float turretYaw = 0.0f;
    weak_ptr<GameObject> target;
};

struct TowerDefenseTowerModelPart
{
    string name;
    shared_ptr<Mesh> mesh;
    vector<vector<shared_ptr<Material>>> materials;
    XMFLOAT4X4 localMatrix{};
    bool rotatesWithTarget = false;
};

struct TowerDefenseGenerator
{
    shared_ptr<GameObject> object;
    XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    int tier = 1;
    float timer = 0.0f;
    float interval = 7.0f;
    int amount = 1;
};

struct TowerDefenseHitMarker
{
    shared_ptr<GameObject> object;
    XMFLOAT3 velocity{ 0.0f, 0.0f, 0.0f };
    float age = 0.0f;
    float lifetime = 0.0f;
    float gravity = 0.0f;
};

struct TowerDefenseDamagePopup
{
    XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 velocity{ 0.0f, 0.0f, 0.0f };
    wstring text;
    float lifetime = 0.0f;
    float maxLifetime = 0.0f;
    float size = 0.025f;
    TowerDefenseTowerType type = TowerDefenseTowerType::Basic;
};

struct TowerDefenseRollingBoulder
{
    shared_ptr<GameObject> object;
    vector<weak_ptr<GameObject>> hitEnemies;
    size_t routeIndex = 0;
    size_t waypointIndex = 1;
    XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 direction{ 1.0f, 0.0f, 0.0f };
    float radius = 0.75f;
    float speed = 4.5f;
    float damage = 80.0f;
    float lifetime = 8.0f;
    float rollAngle = 0.0f;
    int tier = 1;
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
    float arcHeight = 0.0f;
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
    void BuildWaveUI();
    void BuildBossHealthUI();
    void BuildBitmapTextPool();
    void BuildStageTerrainAssets(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList);
    void BuildTowerModelAssets(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList);
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
    void RenderMergeCandidateHighlights(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderTowerInfoUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderStartScreenUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderGoldUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderWavePreviewUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderTowerUpgradeUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderMiniMapUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderBossHealthUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderBossIntroUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderBossRewardUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderWaveRewardUI(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderDamagePopups(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
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
    void UpdateGenerators(float timeElapsed);
    void UpdateProjectiles(float timeElapsed);
    void UpdateHitMarkers(float timeElapsed);
    void UpdateDamagePopups(float timeElapsed);
    void UpdateRollingBoulders(float timeElapsed);
    void UpdateScopeMarkers(float timeElapsed);
    void UpdateDragPreview(HWND hWnd);
    void UpdateEnemyHealthBars();
    void SpawnEnemy(float healthMultiplier = 1.0f,
        float speedMultiplier = 1.0f,
        float visualScale = 1.0f,
        float laneOffset = 0.0f,
        bool isBoss = false,
        bool isFlying = false,
        bool splitsOnDeath = false,
        TowerDefenseEnemyVariant variant = TowerDefenseEnemyVariant::Walker);
    void SpawnEnemyAt(const string& name,
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
        TowerDefenseEnemyVariant variant = TowerDefenseEnemyVariant::Walker);
    void SpawnSplitChildren(const TowerDefenseEnemy& parent, const XMFLOAT3& position);
    void SpawnHitMarker(const XMFLOAT3& position,
        TowerDefenseTowerType type = TowerDefenseTowerType::Basic,
        float scale = 1.0f);
    void SpawnDamagePopup(const XMFLOAT3& position,
        float damage,
        TowerDefenseTowerType type,
        float scale = 1.0f);
    void SpawnDeathEffect(const XMFLOAT3& position, float scale);
    void SpawnCoinDropEffect(const XMFLOAT3& position, int amount);
    void SpawnMergeEffect(const XMFLOAT3& position, TowerDefenseTowerType type, int tier);
    void SpawnMeteorStrike(const XMFLOAT3& position, int tier);
    void SpawnFreezeField(const XMFLOAT3& position, int tier);
    void SpawnBoostEffect(const XMFLOAT3& position, int tier);
    void SpawnRollingBoulder(const XMFLOAT3& position, int tier);
    void SpawnParticle(const XMFLOAT3& position,
        const XMFLOAT3& velocity,
        const XMFLOAT3& scale,
        const shared_ptr<Material>& material,
        float lifetime,
        float gravity = 0.0f);
    void SpawnScopeMarker(const shared_ptr<GameObject>& target, float duration, float radius);
    void SpawnProjectile(const TowerDefenseTower& tower, const shared_ptr<GameObject>& target);
    bool TryPlaceTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryCastMeteor(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryCastFreeze(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryCastBoulder(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryBoostTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryPlaceGenerator(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryMergeOfferWithTower(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer);
    bool TryMergeDraggedTowerWith(const XMFLOAT3& terrainPoint);
    void ApplyTowerStats(TowerDefenseTower& tower);
    void UpdateTowerVisual(TowerDefenseTower& tower);
    void UpdateTowerAimVisual(TowerDefenseTower& tower, const TowerDefenseEnemy* target, float timeElapsed);
    void ApplyTowerModelPartTransforms(TowerDefenseTower& tower) const;
    void RemoveTowerInstanceObjects(TowerDefenseTower& tower);
    void HandleClick(HWND hWnd);
    void BeginTowerDrag(int offerSlot);
    void BeginPlacedTowerDrag(const shared_ptr<GameObject>& towerObject);
    void EndTowerDrag(HWND hWnd);
    bool BuildTowerDragGhostVisual(const TowerDefenseOffer& offer);
    void UpdateTowerDragGhostVisual(const XMFLOAT3& terrainPoint);
    void ClearDragGhost();
    void EnableShadowCasting(const shared_ptr<GameObject>& object) const;
    void RemoveRenderObject(const shared_ptr<GameObject>& object);
    void RemoveSimulationObject(const shared_ptr<GameObject>& object);

    shared_ptr<GameObject> CreateCubeObject(const string& name,
        const XMFLOAT3& position,
        const XMFLOAT3& scale,
        const shared_ptr<Material>& material) const;
    shared_ptr<GameObject> CreateTowerObject(const string& name,
        const XMFLOAT3& terrainPoint,
        TowerDefenseTowerType type,
        int tier) const;
    shared_ptr<GameObject> CreateCapsuleObject(const string& name,
        const XMFLOAT3& position,
        const shared_ptr<Material>& material,
        float scale = 1.0f) const;

    bool TryGetGroundPoint(HWND hWnd, XMFLOAT3& outPoint) const;
    bool TryGetPointOnPlane(HWND hWnd, float planeY, XMFLOAT3& outPoint) const;
    bool TryGetTerrainPoint(HWND hWnd, XMFLOAT3& outPoint) const;
    bool TryGetShopSlotFromCursor(HWND hWnd, int& outSlot) const;
    bool TryHandleStartMenuClick(HWND hWnd);
    bool CanPlaceTower(const XMFLOAT3& terrainPoint) const;
    bool CanMergeOfferAtPoint(const XMFLOAT3& terrainPoint, const TowerDefenseOffer& offer) const;
    bool CanMergeDraggedTowerAtPoint(const XMFLOAT3& terrainPoint) const;
    bool IsNearPath(const XMFLOAT3& terrainPoint, float radius) const;
    bool HasTowerNear(const XMFLOAT3& terrainPoint, float radius) const;
    bool IsEnemyInTowerRange(const TowerDefenseTower& tower, const TowerDefenseEnemy& enemy) const;
    TowerDefenseTower* FindTowerAtPoint(const XMFLOAT3& terrainPoint);
    int FindTowerIndexByObject(const shared_ptr<GameObject>& object) const;
    TowerDefenseEnemy* FindEnemyByObject(const shared_ptr<GameObject>& object);
    TowerDefenseEnemy* AcquireTowerTarget(const TowerDefenseTower& tower);
    const TowerDefenseTower* GetSelectedTower() const;
    const TowerDefenseEnemy* GetActiveBoss() const;
    int GetWaveSize(int wave) const;
    bool IsBossWave(int wave) const;
    int GetRerollCost() const;
    const TowerDefenseTextCache& GetBitmapTextCache(const wstring& text, int pixelHeight) const;
    TowerDefenseTextCache RasterizeBitmapText(const wstring& text, int pixelHeight) const;
    wstring ResolveBitmapFontPath() const;
    XMFLOAT3 TerrainWorldPosition(float localX, float localZ, float yOffset = 0.0f) const;
    float TerrainHeightAtWorldXZ(float x, float z) const;
    void PositionScopeMarker(TowerDefenseScopeMarker& marker) const;
    bool GetShopSlotRect(int tier, float width, float height, XMFLOAT2& outCenter, XMFLOAT2& outHalfSize) const;
    bool IsTowerInfoPanelUnderCursor(HWND hWnd) const;
    bool IsWaveToggleUnderCursor(HWND hWnd) const;
    bool IsShopToggleUnderCursor(HWND hWnd) const;
    bool TryGetTowerUpgradeChoiceFromCursor(HWND hWnd, int& outTypeIndex) const;
    void ToggleWaveRunning();
    void ToggleShopCollapsed();
    int GetTowerDamageUpgradeCost(int typeIndex) const;
    float GetTowerDamageMultiplier(TowerDefenseTowerType type) const;
    float GetGameSpeedMultiplier() const;
    float GetDifficultyHealthMultiplier() const;
    float GetDifficultySpeedMultiplier() const;
    float GetDifficultyRewardMultiplier() const;
    int ScaleReward(int amount) const;
    bool TryUpgradeTowerDamage(int typeIndex);
    bool TryGetBossRewardChoiceFromCursor(HWND hWnd, int& outChoice) const;
    void ApplyBossRewardChoice(int choice);
    void ShowBossReward();
    bool TryGetWaveRewardChoiceFromCursor(HWND hWnd, int& outChoice) const;
    void ApplyWaveRewardChoice(int choice);
    void ShowWaveReward();
    void AdvanceToNextWave();
    void AddGold(int amount);
    XMFLOAT4X4 BuildCameraAnchoredUiMatrix(const XMFLOAT2& normalizedCenter,
        const XMFLOAT2& normalizedSize,
        float depth,
        float thickness) const;
    XMFLOAT4X4 BuildCameraAnchoredModelMatrix(const XMFLOAT2& normalizedCenter,
        float normalizedHeight,
        float depth,
        float yawRadians,
        float pitchRadians = 0.0f) const;
    void RenderTowerModelIcon(const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const shared_ptr<GameObject>& object,
        TowerDefenseTowerType type,
        int tier,
        const XMFLOAT2& normalizedCenter,
        float normalizedHeight,
        float depth,
        float yawRadians = XM_PIDIV4,
        float pitchRadians = 0.0f) const;
    void RenderTierMarkers(const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const XMFLOAT2& normalizedCenter,
        int tier,
        float markerSize,
        float spacing,
        float depth,
        size_t& widgetIndex) const;
    bool WorldToScreenUi(const XMFLOAT3& worldPosition,
        XMFLOAT2& outNormalized,
        float& outDepth) const;
    void ToggleGameplayView();
    void MoveGameplayCamera(const XMFLOAT3& delta);
    void UpdateAngledMouseOrbit(HWND hWnd);
    XMFLOAT3 ClampCameraFocusToTerrain(const XMFLOAT3& focus) const;
    XMFLOAT3 ApplySpringCameraCollision(const XMFLOAT3& focus, const XMFLOAT3& desiredEye) const;
    XMFLOAT4X4 BuildTowerModelRootMatrix(const XMFLOAT3& terrainPoint,
        TowerDefenseTowerType type,
        int tier) const;
    XMFLOAT3 GetTowerProjectileOrigin(const TowerDefenseTower& tower) const;

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
    static constexpr int TowerTypeCount = 6;
    static constexpr int MaxTowerTier = 3;
    static constexpr int MaxTowerDamageUpgradeLevel = 5;
    static constexpr int MaxWave = 6;
    static constexpr int StagePresetCount = 3;
    static constexpr int DifficultyPresetCount = 3;

    TowerDefenseMode m_mode = TowerDefenseMode::StartScreen;
    vector<XMFLOAT3> m_waypoints;
    vector<vector<XMFLOAT3>> m_enemyPaths;
    vector<TowerDefenseTower> m_towers;
    vector<TowerDefenseGenerator> m_generators;
    vector<TowerDefenseEnemy> m_enemies;
    vector<TowerDefenseHitMarker> m_hitMarkers;
    vector<TowerDefenseDamagePopup> m_damagePopups;
    vector<TowerDefenseRollingBoulder> m_rollingBoulders;
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
    shared_ptr<Material> m_bossMaterial;
    shared_ptr<Material> m_flyingEnemyMaterial;
    shared_ptr<Material> m_runnerEnemyMaterial;
    shared_ptr<Material> m_armoredEnemyMaterial;
    shared_ptr<Material> m_splitterEnemyMaterial;
    shared_ptr<Material> m_bossHealthFillMaterial;
    shared_ptr<Material> m_lifeUiMaterial;
    shared_ptr<Material> m_mergeHighlightMaterial;
    shared_ptr<Material> m_meteorMaterial;
    shared_ptr<Material> m_meteorUiMaterial;
    shared_ptr<Material> m_freezeMaterial;
    shared_ptr<Material> m_freezeUiMaterial;
    shared_ptr<Material> m_boostMaterial;
    shared_ptr<Material> m_boostUiMaterial;
    shared_ptr<Material> m_boulderMaterial;
    shared_ptr<Material> m_boulderUiMaterial;
    shared_ptr<Material> m_generatorMaterial;
    shared_ptr<Material> m_generatorUiMaterial;
    shared_ptr<Material> m_shopConsumableSlotMaterial;
    shared_ptr<Material> m_shopGeneratorSlotMaterial;
    shared_ptr<Material> m_shopTowerMaterials[TowerTypeCount][MaxTowerTier];
    shared_ptr<Material> m_towerMaterials[TowerTypeCount][MaxTowerTier];
    shared_ptr<Material> m_towerModelMaterials[TowerTypeCount][MaxTowerTier];
    shared_ptr<Material> m_projectileMaterials[TowerTypeCount];
    shared_ptr<Material> m_hitMaterials[TowerTypeCount];
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
    vector<shared_ptr<TerrainHeightMap>> m_stageHeightMaps;
    vector<shared_ptr<Mesh>> m_stageTerrainMeshes;
    shared_ptr<TerrainCollider> m_terrainCollider;
    shared_ptr<GameObject> m_terrainObject;
    shared_ptr<Mesh> m_towerModelMesh;
    shared_ptr<Mesh> m_boulderMesh;
    vector<TowerDefenseTowerModelPart> m_towerModelParts;
    shared_ptr<GameObject> m_shopPanel;
    vector<shared_ptr<GameObject>> m_shopSlots;
    vector<shared_ptr<GameObject>> m_towerInfoWidgets;
    vector<shared_ptr<GameObject>> m_goldUiWidgets;
    vector<shared_ptr<GameObject>> m_waveUiWidgets;
    vector<shared_ptr<GameObject>> m_bossHealthWidgets;
    vector<shared_ptr<GameObject>> m_hudWidgets;
    vector<shared_ptr<GameObject>> m_selectedTowerRangeMarkers;
    vector<shared_ptr<GameObject>> m_mergeCandidateMarkers;
    mutable vector<shared_ptr<GameObject>> m_bitmapTextRects;
    mutable vector<TowerDefenseTextCache> m_bitmapTextCache;
    mutable size_t m_bitmapTextCursor = 0;
    shared_ptr<GameObject> m_sunObject;
    shared_ptr<PointLight> m_sunLight;
    shared_ptr<GameObject> m_moonObject;
    shared_ptr<PointLight> m_moonLight;

    bool m_rightMouseOrbiting = false;
    bool m_draggingTower = false;
    bool m_draggingPlacedTower = false;
    bool m_shopCollapsed = false;
    bool m_topDownView = false;
    int m_dragOfferSlot = 0;
    int m_dragTier = 1;
    int m_gameSpeedIndex = 0;
    int m_selectedStage = 0;
    int m_selectedDifficulty = 1;
    POINT m_lastOrbitCursor{};
    XMFLOAT3 m_cameraFocus{ 0.0f, 0.0f, 0.0f };
    float m_cameraZoom = 42.0f;
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = DefaultOrbitPitch;
    float m_cameraShakeTimer = 0.0f;
    float m_cameraShakeDuration = 0.0f;
    float m_cameraShakeIntensity = 0.0f;
    float m_bossIntroTimer = 0.0f;
    float m_consumableCooldown = 0.0f;
    float m_consumableCooldownDuration = 1.45f;
    float m_sunCycleTime = 0.0f;
    bool m_bitmapFontLoaded = false;
    wstring m_bitmapFontPath;
    wstring m_bitmapFontFamily = L"Terrarum Sans Bitmap";
    weak_ptr<GameObject> m_selectedTower;
    weak_ptr<GameObject> m_dragSourceTower;
    TowerDefenseOffer m_shopOffers[3];
    TowerDefenseOffer m_dragOffer;
    shared_ptr<GameObject> m_dragGhost;
    vector<TowerDefenseTowerPartInstance> m_dragGhostParts;
    float m_spawnTimer = 0.0f;
    int m_wave = 1;
    int m_spawnedInWave = 0;
    int m_lives = 20;
    int m_maxLives = 20;
    int m_gold = 10;
    bool m_waveRunning = false;
    bool m_bossRewardPending = false;
    int m_bossRewardWave = 0;
    bool m_waveRewardPending = false;
    int m_waveRewardWave = 0;
    int m_totalEnemiesDefeated = 0;
    int m_bossesDefeated = 0;
    int m_towersPlaced = 0;
    int m_towersMerged = 0;
    int m_highestTowerTier = 1;
    int m_goldEarned = 0;
    int m_wavesCleared = 0;
    int m_towerDamageLevels[TowerTypeCount]{};
};
