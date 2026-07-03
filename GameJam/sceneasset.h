#pragma once

#include "stdafx.h"
#include <filesystem>
#include <unordered_map>
#include "mesh.h"
#include "object.h"
#include "collisionmanager.h"
#include "physicsmanager.h"

enum class SceneColliderShape
{
    None,
    Box,
    Capsule,
    Mesh
};

struct SceneColliderData
{
    SceneColliderShape shape = SceneColliderShape::None;
    XMFLOAT3 center{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 boxSize{ 0.0f, 0.0f, 0.0f };
    float radius = 0.5f;
    float height = 1.0f;
    bool hasExplicitBoxSize = false;
};

struct SceneRigidbodyData
{
    bool enabled = false;
    float mass = 1.0f;
    float drag = 0.0f;
    float restitution = 0.0f;
    bool isKinematic = false;
    bool useGravity = true;
    XMFLOAT3 velocity{ 0.0f, 0.0f, 0.0f };
};

struct SceneObjectData
{
    string name;
    string meshName;
    XMFLOAT4X4 worldMatrix{};
    SceneColliderData collider;
    SceneRigidbodyData rigidbody;
};

struct SceneAsset
{
    filesystem::path sourcePath;
    vector<string> meshNames;
    vector<string> materialNames;
    vector<SceneObjectData> objects;
};

class ISceneAssetReader
{
public:
    virtual ~ISceneAssetReader() = default;
    virtual SceneAsset Load(const filesystem::path& scenePath) const = 0;
};

class UnitySceneBinaryReader final : public ISceneAssetReader
{
public:
    SceneAsset Load(const filesystem::path& scenePath) const override;
};

struct SceneBuildResult
{
    vector<shared_ptr<GameObject>> objects;
    vector<shared_ptr<Mesh>> loadedMeshes;
};

class SceneObjectFactory final
{
public:
    shared_ptr<GameObject> Create(const SceneObjectData& data, const shared_ptr<Mesh>& mesh) const;
};

class BinarySceneInstantiator final
{
public:
    SceneBuildResult Build(const filesystem::path& scenePath,
        const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        CollisionManager* collisionManager,
        PhysicsManager* physicsManager) const;
};
