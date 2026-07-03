#pragma once
#include "stdafx.h"
#include "mesh.h"

enum class ColliderType
{
    Box,
    Capsule,
    Mesh,
    Compound,
    Terrain
};

class GameObject;

class Collider
{
public:
    explicit Collider(ColliderType type) : m_type(type) {}
    virtual ~Collider() = default;

    virtual void Update(const XMFLOAT4X4& worldMatrix) = 0;
    virtual XMFLOAT3 GetCenter() const = 0;
    virtual BoundingBox GetWorldAABB() const = 0;
    virtual bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const = 0;

    ColliderType GetType() const { return m_type; }
    virtual void SetOwner(const shared_ptr<GameObject>& owner) { m_owner = owner; }

public:
    weak_ptr<GameObject> m_owner;

protected:
    ColliderType m_type;
};

class BoxCollider : public Collider
{
public:
    explicit BoxCollider(const shared_ptr<Mesh>& mesh) : Collider(ColliderType::Box)
    {
        if (mesh) m_localOBB = mesh->GetLocalOBB();
    }

    BoxCollider(XMFLOAT3 center, XMFLOAT3 size) : Collider(ColliderType::Box)
    {
        m_localOBB.Center = center;
        m_localOBB.Extents = XMFLOAT3{
            max(size.x, 0.001f) * 0.5f,
            max(size.y, 0.001f) * 0.5f,
            max(size.z, 0.001f) * 0.5f };
        m_localOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    explicit BoxCollider(const BoundingOrientedBox& localOBB) : Collider(ColliderType::Box)
    {
        m_localOBB = localOBB;
        m_localOBB.Extents = XMFLOAT3{
            max(m_localOBB.Extents.x, 0.0005f),
            max(m_localOBB.Extents.y, 0.0005f),
            max(m_localOBB.Extents.z, 0.0005f) };
    }

    void Update(const XMFLOAT4X4& worldMatrix) override
    {
        m_localOBB.Transform(m_worldOBB, XMLoadFloat4x4(&worldMatrix));

        XMFLOAT3 corners[8]{};
        m_worldOBB.GetCorners(corners);
        BoundingBox::CreateFromPoints(m_worldAABB, 8, corners, sizeof(XMFLOAT3));
    }

    BoundingOrientedBox GetWorldOBB() const { return m_worldOBB; }
    XMFLOAT3 GetCenter() const override { return m_worldOBB.Center; }
    BoundingBox GetWorldAABB() const override { return m_worldAABB; }
    void SetOffset(XMFLOAT3 offset) { m_localOBB.Center = offset; }

    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const override;

private:
    BoundingOrientedBox m_localOBB{};
    BoundingOrientedBox m_worldOBB{};
    BoundingBox m_worldAABB{};
};

class CapsuleCollider : public Collider
{
public:
    CapsuleCollider(float radius, float height, XMFLOAT3 center = XMFLOAT3{ 0.0f, 0.0f, 0.0f })
        : Collider(ColliderType::Capsule), m_radius(radius), m_height(height), m_localCenter(center) {}

    void Update(const XMFLOAT4X4& worldMatrix) override;
    XMFLOAT3 GetCenter() const override { return m_worldCenter; }
    BoundingBox GetWorldAABB() const override { return m_worldAABB; }
    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const override;

    float GetRadius() const { return m_radius; }
    XMFLOAT3 GetPointA() const { return m_worldPointA; }
    XMFLOAT3 GetPointB() const { return m_worldPointB; }

private:
    float m_radius = 0.5f;
    float m_height = 1.0f;
    XMFLOAT3 m_localCenter{};
    XMFLOAT3 m_worldCenter{};
    XMFLOAT3 m_worldPointA{};
    XMFLOAT3 m_worldPointB{};
    BoundingBox m_worldAABB{};
};

class MeshCollider : public Collider
{
public:
    explicit MeshCollider(const shared_ptr<Mesh>& mesh);

    void Update(const XMFLOAT4X4& worldMatrix) override;
    XMFLOAT3 GetCenter() const override { return m_worldAABB.Center; }
    BoundingBox GetWorldAABB() const override { return m_worldAABB; }
    BoundingOrientedBox GetWorldOBB() const { return m_worldOBB; }
    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const override;

    const vector<MeshTriangle>& GetWorldTriangles() const { return m_worldTriangles; }

private:
    vector<MeshTriangle> m_localTriangles;
    vector<MeshTriangle> m_worldTriangles;
    BoundingOrientedBox m_localOBB{};
    BoundingOrientedBox m_worldOBB{};
    BoundingBox m_worldAABB{};
};

class CompoundCollider : public Collider
{
public:
    struct Child
    {
        string name;
        shared_ptr<Collider> collider;
    };

public:
    CompoundCollider();
    explicit CompoundCollider(const shared_ptr<Mesh>& mesh, float padding = 0.0f);

    void SetOwner(const shared_ptr<GameObject>& owner) override;
    void Update(const XMFLOAT4X4& worldMatrix) override;
    XMFLOAT3 GetCenter() const override { return m_worldAABB.Center; }
    BoundingBox GetWorldAABB() const override { return m_worldAABB; }
    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const override;

    void AddCollider(const string& name, const shared_ptr<Collider>& collider);
    void AddBox(const string& name, XMFLOAT3 center, XMFLOAT3 size);
    void AddBox(const string& name, BoundingOrientedBox localOBB);
    void AddBoxesFromMeshParts(const shared_ptr<Mesh>& mesh, float padding = 0.0f);

    const vector<Child>& GetChildren() const { return m_children; }
    bool IsEmpty() const { return m_children.empty(); }

private:
    void RebuildWorldAABB();

private:
    vector<Child> m_children;
    BoundingBox m_worldAABB{};
};
