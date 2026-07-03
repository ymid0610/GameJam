#pragma once

#include "stdafx.h"
#include "collider.h"

struct CollisionEvent
{
    shared_ptr<Collider> colliderA;
    shared_ptr<Collider> colliderB;
};

struct ContactInfo
{
    XMFLOAT3 normal{};
    XMFLOAT3 surfaceNormal{ 0.0f, 1.0f, 0.0f };
    float penetration = 0.0f;
    bool isTerrainContact = false;
    bool isWalkable = true;
    float slopeAngleDegrees = 0.0f;
};

class CollisionManager
{
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    void Update(bool dispatchEvents = true);
    void AddCollider(const shared_ptr<Collider>& collider);
    void RemoveCollider(const shared_ptr<Collider>& collider);
    void ClearColliders();

    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outHitDist,
        const shared_ptr<Collider>& ignoreCollider = nullptr) const;
    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outHitDist,
        shared_ptr<Collider>& outHitCollider, const shared_ptr<Collider>& ignoreCollider = nullptr) const;
    bool CheckCollision(const shared_ptr<Collider>& a, const shared_ptr<Collider>& b, ContactInfo& outContact);

private:
    void ProcessCollisions();

private:
    vector<shared_ptr<Collider>> m_colliders;
    queue<CollisionEvent> m_eventQueue;
};
