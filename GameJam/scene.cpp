#include "scene.h"
#include "framework.h"
#include "sceneasset.h"
#include "terrain.h"

namespace
{
    constexpr float FirstPersonFlashlightOffset = 0.05f;
    constexpr float ThirdPersonFlashlightForwardOffset = 0.18f;

    struct DebugLineVertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT4 color;
    };

    void AddLine(vector<DebugLineVertex>& vertices, const XMFLOAT3& from, const XMFLOAT3& to, const XMFLOAT4& color)
    {
        constexpr XMFLOAT3 normal{ 0.0f, 1.0f, 0.0f };
        vertices.push_back({ from, normal, color });
        vertices.push_back({ to, normal, color });
    }

    void AddBoxLines(vector<DebugLineVertex>& vertices, const XMFLOAT3 corners[8], const XMFLOAT4& color)
    {
        constexpr int edges[24] = {
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7
        };

        for (int i = 0; i < 24; i += 2)
        {
            AddLine(vertices, corners[edges[i]], corners[edges[i + 1]], color);
        }
    }

    void AddAABBLines(vector<DebugLineVertex>& vertices, const BoundingBox& box, const XMFLOAT4& color)
    {
        XMFLOAT3 corners[8]{};
        box.GetCorners(corners);
        AddBoxLines(vertices, corners, color);
    }

    void AddOBBLines(vector<DebugLineVertex>& vertices, const BoundingOrientedBox& box, const XMFLOAT4& color)
    {
        XMFLOAT3 corners[8]{};
        box.GetCorners(corners);
        AddBoxLines(vertices, corners, color);
    }

    XMFLOAT4 GetCompoundChildDebugColor()
    {
        return XMFLOAT4{ 1.0f, 0.48f, 0.05f, 1.0f };
    }

    bool IsHelicopterPlayer(const shared_ptr<Player>& player)
    {
        return player && player->GetName() == "Apache Helicopter";
    }

    XMFLOAT3 NormalizePlanar(const XMFLOAT3& value, const XMFLOAT3& fallback)
    {
        XMFLOAT3 planar{ value.x, 0.0f, value.z };
        float lengthSq = planar.x * planar.x + planar.z * planar.z;
        if (lengthSq <= Utiles::Physics::Epsilon) return fallback;

        float invLength = 1.0f / sqrtf(lengthSq);
        return XMFLOAT3{ planar.x * invLength, 0.0f, planar.z * invLength };
    }

    void HandleHelicopterPlayerInput(const shared_ptr<Player>& player, FLOAT timeElapsed)
    {
        if (!player) return;

        constexpr float turnSpeed = 80.0f;

        bool turnLeftKey = (GetAsyncKeyState('Q') & 0x8000) != 0;
        bool turnRightKey = (GetAsyncKeyState('E') & 0x8000) != 0;

        float yaw = 0.0f;
        if (turnLeftKey) yaw -= turnSpeed * timeElapsed;
        if (turnRightKey) yaw += turnSpeed * timeElapsed;
        if (yaw != 0.0f) player->Rotate(0.0f, yaw, 0.0f);
    }

    XMFLOAT3 StoreVector3(FXMVECTOR value)
    {
        XMFLOAT3 result{};
        XMStoreFloat3(&result, value);
        return result;
    }

    void AddCircleLines(vector<DebugLineVertex>& vertices, FXMVECTOR center, FXMVECTOR right, FXMVECTOR forward,
        float radius, int segments, const XMFLOAT4& color)
    {
        XMVECTOR previous{};

        for (int i = 0; i <= segments; ++i)
        {
            float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(segments);
            XMVECTOR offset = XMVectorAdd(
                XMVectorScale(right, cosf(angle) * radius),
                XMVectorScale(forward, sinf(angle) * radius));
            XMVECTOR current = XMVectorAdd(center, offset);

            if (i > 0)
            {
                AddLine(vertices, StoreVector3(previous), StoreVector3(current), color);
            }

            previous = current;
        }
    }

    void AddHemisphereArc(vector<DebugLineVertex>& vertices, FXMVECTOR center, FXMVECTOR axis, FXMVECTOR radial,
        float radius, bool upper, int segments, const XMFLOAT4& color)
    {
        XMVECTOR previous{};

        for (int i = 0; i <= segments; ++i)
        {
            float angle = XM_PI * static_cast<float>(i) / static_cast<float>(segments);
            float axisSign = upper ? 1.0f : -1.0f;

            XMVECTOR offset = XMVectorAdd(
                XMVectorScale(radial, cosf(angle) * radius),
                XMVectorScale(axis, sinf(angle) * radius * axisSign));
            XMVECTOR current = XMVectorAdd(center, offset);

            if (i > 0)
            {
                AddLine(vertices, StoreVector3(previous), StoreVector3(current), color);
            }

            previous = current;
        }
    }

    void AddCapsuleLines(vector<DebugLineVertex>& vertices, const CapsuleCollider& capsule, const XMFLOAT4& color)
    {
        constexpr int RingSegments = 24;
        constexpr int ArcSegments = 12;

        XMFLOAT3 pointAFloat = capsule.GetPointA();
        XMFLOAT3 pointBFloat = capsule.GetPointB();
        XMVECTOR pointA = XMLoadFloat3(&pointAFloat);
        XMVECTOR pointB = XMLoadFloat3(&pointBFloat);
        XMVECTOR axis = XMVectorSubtract(pointA, pointB);

        if (Utiles::Physics::VectorLengthSq(axis) <= Utiles::Physics::Epsilon) return;

        axis = XMVector3Normalize(axis);
        XMVECTOR reference = fabsf(Utiles::Physics::VectorDot(axis, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))) < 0.95f
            ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
            : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(reference, axis));
        XMVECTOR forward = XMVector3Normalize(XMVector3Cross(axis, right));

        const float radius = capsule.GetRadius();

        AddCircleLines(vertices, pointA, right, forward, radius, RingSegments, color);
        AddCircleLines(vertices, pointB, right, forward, radius, RingSegments, color);

        XMVECTOR radialDirections[4] = {
            right,
            XMVectorScale(right, -1.0f),
            forward,
            XMVectorScale(forward, -1.0f)
        };

        for (const auto& radial : radialDirections)
        {
            AddLine(vertices,
                StoreVector3(XMVectorAdd(pointA, XMVectorScale(radial, radius))),
                StoreVector3(XMVectorAdd(pointB, XMVectorScale(radial, radius))),
                color);
        }

        AddHemisphereArc(vertices, pointA, axis, right, radius, true, ArcSegments, color);
        AddHemisphereArc(vertices, pointA, axis, forward, radius, true, ArcSegments, color);
        AddHemisphereArc(vertices, pointB, axis, right, radius, false, ArcSegments, color);
        AddHemisphereArc(vertices, pointB, axis, forward, radius, false, ArcSegments, color);
    }

    void AddMeshTriangleLines(vector<DebugLineVertex>& vertices, const MeshCollider& mesh, const XMFLOAT4& color)
    {
        for (const auto& triangle : mesh.GetWorldTriangles())
        {
            AddLine(vertices, triangle.a, triangle.b, color);
            AddLine(vertices, triangle.b, triangle.c, color);
            AddLine(vertices, triangle.c, triangle.a, color);
        }
    }

    void AddTerrainTriangleLines(vector<DebugLineVertex>& vertices, const TerrainCollider& terrain, const XMFLOAT4& color)
    {
        const auto& heightMap = terrain.GetHeightMap();
        if (!heightMap || heightMap->GetWidth() < 2 || heightMap->GetLength() < 2) return;

        UINT width = heightMap->GetWidth();
        UINT length = heightMap->GetLength();
        size_t horizontalLines = static_cast<size_t>(width - 1) * length;
        size_t verticalLines = static_cast<size_t>(length - 1) * width;
        size_t diagonalLines = static_cast<size_t>(width - 1) * (length - 1);
        vertices.reserve(vertices.size() + (horizontalLines + verticalLines + diagonalLines) * 2);

        vector<XMFLOAT3> worldPoints(static_cast<size_t>(width) * length);
        XMMATRIX world = XMLoadFloat4x4(&terrain.GetWorldMatrix());
        float spacing = heightMap->GetCellSpacing();

        auto index = [width](UINT x, UINT z)
        {
            return static_cast<size_t>(z) * width + x;
        };

        for (UINT z = 0; z < length; ++z)
        {
            float localZ = static_cast<float>(z) * spacing;
            for (UINT x = 0; x < width; ++x)
            {
                float localX = static_cast<float>(x) * spacing;
                XMFLOAT3 localPoint{ localX, heightMap->SampleHeight(localX, localZ), localZ };
                XMStoreFloat3(&worldPoints[index(x, z)],
                    XMVector3TransformCoord(XMLoadFloat3(&localPoint), world));
            }
        }

        auto point = [&worldPoints, &index](UINT x, UINT z) -> const XMFLOAT3&
        {
            return worldPoints[index(x, z)];
        };

        for (UINT z = 0; z < length; ++z)
        {
            for (UINT x = 0; x + 1 < width; ++x)
            {
                AddLine(vertices, point(x, z), point(x + 1, z), color);
            }
        }

        for (UINT z = 0; z + 1 < length; ++z)
        {
            for (UINT x = 0; x < width; ++x)
            {
                AddLine(vertices, point(x, z), point(x, z + 1), color);
            }
        }

        for (UINT z = 0; z + 1 < length; ++z)
        {
            for (UINT x = 0; x + 1 < width; ++x)
            {
                AddLine(vertices, point(x + 1, z), point(x, z + 1), color);
            }
        }
    }

    XMFLOAT4X4 BuildCameraAnchoredMatrix(const shared_ptr<Camera>& camera)
    {
        const XMFLOAT3 right = camera->GetU();
        const XMFLOAT3 up = camera->GetV();
        const XMFLOAT3 forward = camera->GetN();
        const XMFLOAT3 eye = camera->GetEye();

        return XMFLOAT4X4{
            right.x, right.y, right.z, 0.0f,
            up.x, up.y, up.z, 0.0f,
            forward.x, forward.y, forward.z, 0.0f,
            eye.x, eye.y, eye.z, 1.0f
        };
    }

    filesystem::path GetExecutableDirectory()
    {
        WCHAR modulePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
        if (length == 0 || length == _countof(modulePath)) return {};

        return filesystem::path(modulePath).parent_path();
    }

    filesystem::path ResolveContentPath(const string& contentPath)
    {
        filesystem::path requested(contentPath);
        if (filesystem::exists(requested)) return requested;

        filesystem::path executableDirectory = GetExecutableDirectory();
        if (!executableDirectory.empty())
        {
            const filesystem::path candidates[] = {
                executableDirectory / requested,
                executableDirectory / ".." / requested,
                executableDirectory / ".." / "01. Framework" / requested,
                executableDirectory / ".." / ".." / "01. Framework" / requested
            };

            for (const auto& candidate : candidates)
            {
                if (filesystem::exists(candidate)) return filesystem::absolute(candidate);
            }
        }

        return {};
    }
}

Scene::Scene()
{
}

void Scene::Update(FLOAT timeElapsed)
{
    if (m_player) m_player->Update(timeElapsed);

    for (auto& object : m_objects)
    {
        if (object) object->Update(timeElapsed);
    }

    if (m_physicsManager && m_collisionManager)
    {
        constexpr int substeps = Utiles::Physics::PhysicsSubsteps;
        float substepTime = timeElapsed / static_cast<float>(substeps);
        for (int i = 0; i < substeps; ++i)
        {
            m_physicsManager->Update(substepTime);
            m_collisionManager->Update(i == substeps - 1);
        }
    }
    else
    {
        if (m_physicsManager) m_physicsManager->Update(timeElapsed);
        if (m_collisionManager) m_collisionManager->Update();
    }
    if (m_camera && m_player) m_camera->UpdateEye(m_player->GetPosition());
    UpdateBullets(timeElapsed);
    UpdateFlashlight();
}

void Scene::UpdateSpringCamera()
{
    auto springCamera = dynamic_pointer_cast<SpringArmCamera>(m_camera);
    if (!springCamera || !m_collisionManager || !m_player || !m_player->GetCollider()) return;

    XMFLOAT3 origin = springCamera->GetLookAtPosition();
    XMFLOAT3 direction = springCamera->GetDirectionToCamera();
    float maxDistance = springCamera->GetArmLength();
    float hitDist = 0.0f;

    constexpr float MinimumSpringArmHitDistance = 0.35f;
    if (m_collisionManager->Raycast(origin, direction, hitDist, m_player->GetCollider()) &&
        hitDist > MinimumSpringArmHitDistance &&
        hitDist < maxDistance)
    {
        springCamera->SetCollisionDistance(hitDist);
    }
}

void Scene::MouseEvent(HWND hWnd, FLOAT timeElapsed)
{
    if (!m_camera) return;

    SetCursor(NULL);

    RECT windowRect{};
    GetWindowRect(hWnd, &windowRect);

    POINT center{
        windowRect.left + static_cast<LONG>(g_framework->GetWindowWidth() / 2),
        windowRect.top + static_cast<LONG>(g_framework->GetWindowHeight() / 2)
    };

    POINT mousePosition{};
    GetCursorPos(&mousePosition);

    float dx = XMConvertToRadians(0.15f * static_cast<FLOAT>(center.x - mousePosition.x));
    float dy = XMConvertToRadians(0.15f * static_cast<FLOAT>(center.y - mousePosition.y));

    if (!m_debugCameraEnabled && m_player && !IsHelicopterPlayer(m_player))
    {
        auto springCamera = dynamic_pointer_cast<SpringArmCamera>(m_camera);
        if (m_camera->IsFirstPerson() || (springCamera && springCamera->GetArmLength() <= 1.0f))
        {
            m_player->Rotate(0.0f, XMConvertToDegrees(-dx), 0.0f);
        }
    }

    m_camera->RotateYaw(dx);
    m_camera->RotatePitch(dy);
    SetCursorPos(center.x, center.y);

    if (!m_debugCameraEnabled && m_player) m_player->MouseEvent(timeElapsed, 0);
}

void Scene::KeyboardEvent(FLOAT timeElapsed)
{
    HandleSceneShortcuts();
    if (m_debugCameraEnabled)
    {
        UpdateDebugCamera(timeElapsed);
        return;
    }

    if (IsHelicopterPlayer(m_player)) HandleHelicopterPlayerInput(m_player, timeElapsed);
    else if (m_player) m_player->KeyboardEvent(timeElapsed);
}

bool Scene::OnKeyDown(WPARAM wParam)
{
    (void)wParam;
    return false;
}

void Scene::HandleSceneShortcuts()
{
    bool flashlightKeyDown = (GetAsyncKeyState('F') & 0x8000) != 0;
    bool cameraToggleKeyDown = (GetAsyncKeyState('V') & 0x8000) != 0;
    bool debugCameraKeyDown = (GetAsyncKeyState('B') & 0x8000) != 0;

    if (flashlightKeyDown && !m_flashlightKeyDown) ToggleFlashlight();
    if (cameraToggleKeyDown && !m_cameraToggleKeyDown && !m_debugCameraEnabled) ToggleCameraMode();
    if (debugCameraKeyDown && !m_debugCameraKeyDown) ToggleDebugCamera();
    if (!m_debugCameraEnabled) HandleFireInput();
    else m_fireKeyDown = false;

    m_flashlightKeyDown = flashlightKeyDown;
    m_cameraToggleKeyDown = cameraToggleKeyDown;
    m_debugCameraKeyDown = debugCameraKeyDown;
}

void Scene::ToggleFlashlight()
{
    if (m_flashlight) m_flashlight->SetEnabled(!m_flashlight->IsEnabled());
}

void Scene::ToggleCameraMode()
{
    if (!m_camera || m_debugCameraEnabled) return;

    SetActiveCamera(m_camera->IsFirstPerson() ? CreateThirdPersonCamera() : CreateFirstPersonCamera());
}

void Scene::SetActiveCamera(const shared_ptr<Camera>& camera)
{
    if (!camera) return;

    ConfigureCameraLens(camera);
    m_camera = camera;

    if (m_player)
    {
        m_player->SetCamera(m_camera);
        m_camera->UpdateEye(m_player->GetPosition());
        UpdateFlashlight();
    }
}

shared_ptr<Camera> Scene::CreateFirstPersonCamera() const
{
    return make_shared<FirstPersonCamera>();
}

shared_ptr<Camera> Scene::CreateThirdPersonCamera() const
{
    return make_shared<SpringArmCamera>();
}

void Scene::ConfigureCameraLens(const shared_ptr<Camera>& camera) const
{
    if (!camera || !g_framework) return;
    camera->SetLens(0.25f * XM_PI, g_framework->GetAspectRatio(), 0.1f, 1000.0f);
}

void Scene::ToggleDebugCamera()
{
    if (m_debugCameraEnabled)
    {
        m_debugCameraEnabled = false;
        SetActiveCamera(m_savedGameplayCamera ? m_savedGameplayCamera : CreateFirstPersonCamera());
        m_savedGameplayCamera.reset();
        return;
    }

    if (!m_camera) return;

    m_savedGameplayCamera = m_camera;
    auto spectatorCamera = make_shared<SpectatorCamera>();
    ConfigureCameraLens(spectatorCamera);
    spectatorCamera->SetPose(m_camera->GetEye(), m_camera->GetN());
    m_camera = spectatorCamera;
    m_debugCameraEnabled = true;
}

void Scene::UpdateDebugCamera(FLOAT timeElapsed)
{
    auto spectatorCamera = dynamic_pointer_cast<SpectatorCamera>(m_camera);
    if (!spectatorCamera) return;

    XMFLOAT3 move{ 0.0f, 0.0f, 0.0f };
    const XMFLOAT3 forward = m_camera->GetN();
    const XMFLOAT3 right = m_camera->GetU();
    const XMFLOAT3 up{ 0.0f, 1.0f, 0.0f };

    if (GetAsyncKeyState('W') & 0x8000) move = Utiles::Vector3::Add(move, forward);
    if (GetAsyncKeyState('S') & 0x8000) move = Utiles::Vector3::Sub(move, forward);
    if (GetAsyncKeyState('D') & 0x8000) move = Utiles::Vector3::Add(move, right);
    if (GetAsyncKeyState('A') & 0x8000) move = Utiles::Vector3::Sub(move, right);
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) move = Utiles::Vector3::Add(move, up);
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) move = Utiles::Vector3::Sub(move, up);

    float lengthSq = Utiles::Vector3::Dot(move, move);
    if (lengthSq <= Utiles::Physics::Epsilon) return;

    float speed = Settings::DebugCameraSpeed;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) speed *= Settings::DebugCameraFastMultiplier;

    move = Utiles::Vector3::Mul(Utiles::Vector3::Normalize(move), speed * timeElapsed);
    spectatorCamera->Move(move);
}

void Scene::HandleFireInput()
{
    bool fireKeyDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (fireKeyDown && !m_fireKeyDown) FireBullet();
    m_fireKeyDown = fireKeyDown;
}

void Scene::FireBullet()
{
    if (!m_bulletMesh || !m_player) return;

    XMFLOAT3 direction = GetBulletDirection();
    XMFLOAT3 spawnPosition = GetBulletSpawnPosition(direction);

    if (m_bullets.size() >= Settings::MaxBullets)
    {
        m_bullets.erase(m_bullets.begin());
    }

    m_bullets.push_back(make_shared<Bullet>(
        m_bulletMesh,
        spawnPosition,
        direction,
        Settings::BulletSpeed,
        Settings::BulletLifetime));
}

void Scene::UpdateBullets(FLOAT timeElapsed)
{
    const auto playerCollider = m_player ? m_player->GetCollider() : nullptr;

    for (const auto& bullet : m_bullets)
    {
        if (!bullet || bullet->IsExpired()) continue;

        bullet->Update(timeElapsed);

        if (!m_collisionManager) continue;

        float hitDistance = 0.0f;
        shared_ptr<Collider> hitCollider;
        bool hit = m_collisionManager->Raycast(
            bullet->GetPreviousPosition(),
            bullet->GetDirection(),
            hitDistance,
            hitCollider,
            playerCollider);

        if (hit && hitDistance <= bullet->GetLastTravelDistance() + Settings::BulletRadius)
        {
            bullet->MarkExpired();
            OnBulletHit(bullet, hitCollider);
        }
    }

    erase_if(m_bullets, [](const shared_ptr<Bullet>& bullet)
        {
            return !bullet || bullet->IsExpired();
        });
}

void Scene::OnBulletHit(const shared_ptr<Bullet>& bullet, const shared_ptr<Collider>& hitCollider)
{
    (void)bullet;
    (void)hitCollider;
}

void Scene::UpdateFlashlight()
{
    if (!m_flashlight) return;

    m_flashlight->SetPosition(GetFlashlightPosition());
    m_flashlight->SetDirection(GetFlashlightDirection());
}

XMFLOAT3 Scene::GetBulletSpawnPosition(const XMFLOAT3& direction) const
{
    if (m_camera && m_camera->IsFirstPerson())
    {
        return Utiles::Vector3::Add(m_camera->GetEye(), Utiles::Vector3::Mul(direction, 0.65f));
    }

    if (m_player)
    {
        XMFLOAT3 eyePosition = Utiles::Vector3::Add(
            m_player->GetPosition(),
            XMFLOAT3{ 0.0f, Settings::FirstPersonEyeHeight * 0.65f, 0.0f });
        return Utiles::Vector3::Add(eyePosition, Utiles::Vector3::Mul(direction, Settings::CapsuleRadius + 0.35f));
    }

    return XMFLOAT3{ 0.0f, 0.0f, 0.0f };
}

XMFLOAT3 Scene::GetBulletDirection() const
{
    if (m_camera && m_camera->IsFirstPerson()) return m_camera->GetN();
    if (m_player) return Utiles::Vector3::Normalize(m_player->GetFront());
    return m_camera ? m_camera->GetN() : XMFLOAT3{ 0.0f, 0.0f, 1.0f };
}

XMFLOAT3 Scene::GetFlashlightPosition() const
{
    if (m_camera && m_camera->IsFirstPerson())
    {
        return Utiles::Vector3::Add(
            m_camera->GetEye(),
            Utiles::Vector3::Mul(m_camera->GetN(), FirstPersonFlashlightOffset));
    }

    if (m_player)
    {
        XMFLOAT3 eyePosition = Utiles::Vector3::Add(
            m_player->GetPosition(),
            XMFLOAT3{ 0.0f, Settings::FirstPersonEyeHeight, 0.0f });
        return Utiles::Vector3::Add(
            eyePosition,
            Utiles::Vector3::Mul(GetFlashlightDirection(), ThirdPersonFlashlightForwardOffset));
    }

    return m_camera ? m_camera->GetEye() : XMFLOAT3{ 0.0f, 0.0f, 0.0f };
}

XMFLOAT3 Scene::GetFlashlightDirection() const
{
    if (m_camera && m_camera->IsFirstPerson()) return m_camera->GetN();
    if (m_player) return Utiles::Vector3::Normalize(m_player->GetFront());
    return m_camera ? m_camera->GetN() : XMFLOAT3{ 0.0f, 0.0f, 1.0f };
}

void Scene::MouseWheelEvent(WPARAM wParam)
{
    if (!m_player) return;

    short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    m_player->MouseEvent(0.0f, wheelDelta);
}

void Scene::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_camera) m_camera->UpdateShaderVariable(commandList);
    if (m_shader)
    {
        vector<LightShaderData> lightData;
        lightData.reserve(m_lights.size());
        for (const auto& light : m_lights)
        {
            if (light) lightData.push_back(light->BuildShaderData());
        }

        m_shader->SetLights(lightData);
        m_shader->UpdateShaderVariable(commandList);
    }

    for (const auto& object : m_objects)
    {
        if (object) object->Render(commandList);
    }

    for (const auto& bullet : m_bullets)
    {
        if (bullet) bullet->Render(commandList);
    }

    const bool isFirstPerson = m_camera && m_camera->IsFirstPerson();
    if (m_player && !isFirstPerson) m_player->Render(commandList);
    if (isFirstPerson) RenderFirstPersonOverlay(commandList);
    if (m_debugCameraEnabled) RenderDebugColliders(commandList);
}

void Scene::RenderFirstPersonOverlay(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (!m_camera || !m_overlayShader) return;

    const XMFLOAT4X4 cameraAnchoredMatrix = BuildCameraAnchoredMatrix(m_camera);
    m_overlayShader->UpdateShaderVariable(commandList);

    if (m_firstPersonGun)
    {
        m_firstPersonGun->SetWorldMatrix(cameraAnchoredMatrix);
        m_firstPersonGun->Render(commandList);
    }

    if (m_crosshair)
    {
        m_crosshair->SetWorldMatrix(cameraAnchoredMatrix);
        m_crosshair->Render(commandList);
    }
}

void Scene::RenderDebugColliders(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (!m_debugLineShader || !m_device) return;

    vector<DebugLineVertex> vertices;
    vertices.reserve((m_objects.size() + (m_player ? 1 : 0)) * 160);

    auto appendCollider = [&vertices](const shared_ptr<Collider>& collider)
    {
        if (!collider) return;

        switch (collider->GetType())
        {
        case ColliderType::Box:
        {
            auto box = static_pointer_cast<BoxCollider>(collider);
            AddOBBLines(vertices, box->GetWorldOBB(), XMFLOAT4{ 0.82f, 0.86f, 0.90f, 1.0f });
            break;
        }
        case ColliderType::Capsule:
        {
            auto capsule = static_pointer_cast<CapsuleCollider>(collider);
            AddCapsuleLines(vertices, *capsule, XMFLOAT4{ 0.2f, 1.0f, 0.25f, 1.0f });
            break;
        }
        case ColliderType::Mesh:
        {
            auto mesh = static_pointer_cast<MeshCollider>(collider);
            AddMeshTriangleLines(vertices, *mesh, XMFLOAT4{ 1.0f, 0.85f, 0.1f, 1.0f });
            break;
        }
        case ColliderType::Compound:
        {
            auto compound = static_pointer_cast<CompoundCollider>(collider);
            for (const auto& child : compound->GetChildren())
            {
                if (!child.collider) continue;

                XMFLOAT4 childColor = GetCompoundChildDebugColor();
                if (child.collider->GetType() == ColliderType::Box)
                {
                    auto box = static_pointer_cast<BoxCollider>(child.collider);
                    AddOBBLines(vertices, box->GetWorldOBB(), childColor);
                }
                else if (child.collider->GetType() == ColliderType::Capsule)
                {
                    auto capsule = static_pointer_cast<CapsuleCollider>(child.collider);
                    AddCapsuleLines(vertices, *capsule, childColor);
                }
                else
                {
                    AddAABBLines(vertices, child.collider->GetWorldAABB(), childColor);
                }
            }
            break;
        }
        case ColliderType::Terrain:
        {
            auto terrain = static_pointer_cast<TerrainCollider>(collider);
            AddTerrainTriangleLines(vertices, *terrain, XMFLOAT4{ 0.25f, 1.0f, 0.45f, 1.0f });
            break;
        }
        default:
            AddAABBLines(vertices, collider->GetWorldAABB(), XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
            break;
        }
    };

    if (m_player) appendCollider(m_player->GetCollider());
    for (const auto& object : m_objects)
    {
        if (object) appendCollider(object->GetCollider());
    }

    if (vertices.empty()) return;

    EnsureDebugLineCapacity(static_cast<UINT>(vertices.size()));
    if (!m_mappedDebugLineData) return;
    memcpy(m_mappedDebugLineData, vertices.data(), vertices.size() * sizeof(DebugLineVertex));

    m_debugLineShader->UpdateShaderVariable(commandList);

    XMFLOAT4X4 identity{};
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    commandList->SetGraphicsRoot32BitConstants(0, 16, &identity, 0);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->IASetVertexBuffers(0, 1, &m_debugLineVertexBufferView);
    commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
}

void Scene::EnsureDebugLineCapacity(UINT vertexCount) const
{
    if (vertexCount == 0 || !m_device) return;
    if (m_debugLineVertexBuffer && vertexCount <= m_debugLineCapacity) return;

    if (m_debugLineVertexBuffer && m_mappedDebugLineData)
    {
        m_debugLineVertexBuffer->Unmap(0, nullptr);
        m_mappedDebugLineData = nullptr;
    }

    m_debugLineCapacity = max(vertexCount, max(m_debugLineCapacity * 2, 256u));
    const UINT bufferSize = m_debugLineCapacity * sizeof(DebugLineVertex);

    Utiles::ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_debugLineVertexBuffer)));

    Utiles::ThrowIfFailed(m_debugLineVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedDebugLineData)));

    m_debugLineVertexBufferView.BufferLocation = m_debugLineVertexBuffer->GetGPUVirtualAddress();
    m_debugLineVertexBufferView.SizeInBytes = bufferSize;
    m_debugLineVertexBufferView.StrideInBytes = sizeof(DebugLineVertex);
}

void Scene::ReleaseDebugLineBuffer()
{
    if (m_debugLineVertexBuffer && m_mappedDebugLineData)
    {
        m_debugLineVertexBuffer->Unmap(0, nullptr);
        m_mappedDebugLineData = nullptr;
    }

    m_debugLineVertexBuffer.Reset();
    m_debugLineVertexBufferView = D3D12_VERTEX_BUFFER_VIEW{};
    m_debugLineCapacity = 0;
}

void Scene::BuildObjects(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const ComPtr<ID3D12RootSignature>& rootSignature)
{
    (void)device;
    (void)commandList;
    (void)rootSignature;
}

bool Scene::LoadBinaryScene(const string& scenePath,
    const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    filesystem::path path = ResolveContentPath(scenePath);
    if (path.empty()) return false;

    try
    {
        BinarySceneInstantiator instantiator;
        SceneBuildResult result = instantiator.Build(path, device, commandList, m_collisionManager.get(), m_physicsManager.get());

        m_objects.insert(m_objects.end(), result.objects.begin(), result.objects.end());
        m_binarySceneMeshes.insert(m_binarySceneMeshes.end(), result.loadedMeshes.begin(), result.loadedMeshes.end());
        return !result.objects.empty();
    }
    catch (const exception& e)
    {
        string message = "Binary scene load failed: " + string(e.what()) + "\n";
        OutputDebugStringA(message.c_str());
        return false;
    }
}

void Scene::ReleaseObjects()
{
    m_objects.clear();
    m_player.reset();
    m_camera.reset();
    m_savedGameplayCamera.reset();
    m_shader.reset();
    m_overlayShader.reset();
    m_debugLineShader.reset();
    m_cube.reset();
    m_capsuleMesh.reset();
    m_firstPersonGunMesh.reset();
    m_crosshairMesh.reset();
    m_bulletMesh.reset();
    m_stairMesh.reset();
    m_terrainMesh.reset();
    m_binarySceneMeshes.clear();
    m_firstPersonGun.reset();
    m_crosshair.reset();
    m_bullets.clear();
    m_collisionManager.reset();
    m_physicsManager.reset();
    m_lights.clear();
    m_mainLight.reset();
    m_flashlight.reset();
    ReleaseDebugLineBuffer();
    m_device.Reset();
}

void Scene::ReleaseUploadBuffer()
{
    if (m_cube) m_cube->ReleaseUploadBuffer();
    if (m_capsuleMesh) m_capsuleMesh->ReleaseUploadBuffer();
    if (m_firstPersonGunMesh) m_firstPersonGunMesh->ReleaseUploadBuffer();
    if (m_crosshairMesh) m_crosshairMesh->ReleaseUploadBuffer();
    if (m_bulletMesh) m_bulletMesh->ReleaseUploadBuffer();
    if (m_stairMesh) m_stairMesh->ReleaseUploadBuffer();
    if (m_terrainMesh) m_terrainMesh->ReleaseUploadBuffer();
    for (const auto& mesh : m_binarySceneMeshes)
    {
        if (mesh) mesh->ReleaseUploadBuffer();
    }
}
