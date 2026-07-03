#pragma once
#include "stdafx.h"
#include "shader.h"
#include "mesh.h"
#include "object.h"
#include "player.h"
#include "camera.h"
#include "collisionmanager.h"
#include "physicsmanager.h"
#include "light.h"
#include "bullet.h"
#include "shadow.h"

class Scene
{
public:
    Scene();
    virtual ~Scene() = default;

    virtual void Update(FLOAT timeElapsed);
    virtual void RenderShadowMap(const ComPtr<ID3D12GraphicsCommandList>& commandList);
    virtual void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    virtual void BuildObjects(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const ComPtr<ID3D12RootSignature>& rootSignature);

    virtual void ReleaseObjects();
    virtual void ReleaseUploadBuffer();
    void UpdateSpringCamera();

    virtual void MouseEvent(HWND hWnd, FLOAT timeElapsed);
    virtual void MouseButtonEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    virtual void KeyboardEvent(FLOAT timeElapsed);
    virtual bool OnKeyDown(WPARAM wParam);
    virtual void MouseWheelEvent(WPARAM wParam);

protected:
    void HandleSceneShortcuts();
    void ToggleFlashlight();
    void ToggleCameraMode();
    void SetActiveCamera(const shared_ptr<Camera>& camera);
    shared_ptr<Camera> CreateFirstPersonCamera() const;
    shared_ptr<Camera> CreateThirdPersonCamera() const;
    void ConfigureCameraLens(const shared_ptr<Camera>& camera) const;
    void HandleFireInput();
    void ToggleDebugCamera();
    void UpdateDebugCamera(FLOAT timeElapsed);
    void FireBullet();
    void UpdateBullets(FLOAT timeElapsed);
    virtual void OnBulletHit(const shared_ptr<Bullet>& bullet, const shared_ptr<Collider>& hitCollider);
    void UpdateFlashlight();
    XMFLOAT3 GetBulletSpawnPosition(const XMFLOAT3& direction) const;
    XMFLOAT3 GetBulletDirection() const;
    XMFLOAT3 GetFlashlightPosition() const;
    XMFLOAT3 GetFlashlightDirection() const;
    void RenderFirstPersonOverlay(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void RenderDebugColliders(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void EnsureDebugLineCapacity(UINT vertexCount) const;
    void ReleaseDebugLineBuffer();
    bool LoadBinaryScene(const string& scenePath,
        const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList);

protected:
    shared_ptr<Shader> m_shader;
    shared_ptr<Shader> m_overlayShader;
    shared_ptr<Shader> m_debugLineShader;
    shared_ptr<Camera> m_camera;
    shared_ptr<Camera> m_savedGameplayCamera;
    shared_ptr<Player> m_player;
    vector<shared_ptr<GameObject>> m_objects;
    shared_ptr<Mesh> m_cube;
    shared_ptr<Mesh> m_capsuleMesh;
    shared_ptr<Mesh> m_firstPersonGunMesh;
    shared_ptr<Mesh> m_crosshairMesh;
    shared_ptr<Mesh> m_bulletMesh;
    shared_ptr<Mesh> m_stairMesh;
    shared_ptr<Mesh> m_terrainMesh;
    vector<shared_ptr<Mesh>> m_binarySceneMeshes;
    shared_ptr<GameObject> m_firstPersonGun;
    shared_ptr<GameObject> m_crosshair;
    vector<shared_ptr<Bullet>> m_bullets;
    unique_ptr<ShadowMap> m_shadowMap;

    unique_ptr<CollisionManager> m_collisionManager;
    unique_ptr<PhysicsManager> m_physicsManager;
    vector<shared_ptr<Light>> m_lights;
    shared_ptr<PointLight> m_mainLight;
    shared_ptr<SpotLight> m_flashlight;
    ComPtr<ID3D12Device> m_device;
    mutable ComPtr<ID3D12Resource> m_debugLineVertexBuffer;
    mutable D3D12_VERTEX_BUFFER_VIEW m_debugLineVertexBufferView{};
    mutable UINT8* m_mappedDebugLineData = nullptr;
    mutable UINT m_debugLineCapacity = 0;

    bool m_flashlightKeyDown = false;
    bool m_cameraToggleKeyDown = false;
    bool m_fireKeyDown = false;
    bool m_debugCameraEnabled = false;
    bool m_debugCameraKeyDown = false;
};

class TestScene : public Scene
{
public:
    TestScene();
    ~TestScene() = default;

    void Update(FLOAT timeElapsed) override;
    void ReleaseObjects() override;
    void BuildObjects(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const ComPtr<ID3D12RootSignature>& rootSignature) override;

private:
    void UpdateHelicopterRotorAndLift(FLOAT timeElapsed);
    void UpdateHelicopterRotorVisual();

private:
    shared_ptr<GameObject> m_mainRotor;
    shared_ptr<GameObject> m_tailRotor;
    FLOAT m_mainRotorSpeed = 0.0f;
    FLOAT m_mainRotorAngle = 0.0f;
    FLOAT m_mainRotorTiltPitch = 0.0f;
    FLOAT m_mainRotorTiltRoll = 0.0f;
    FLOAT m_tailRotorSpeed = 0.0f;
    FLOAT m_tailRotorAngle = 0.0f;
    FLOAT m_tailRotorDirection = 1.0f;
};
