#pragma once
#include "stdafx.h"

class GameObject;

enum class ForceMode
{
    Force,
    Impulse,
    VelocityChange
};

enum class CollisionDetectionMode
{
    Discrete,
    ContinuousDynamic
};

class Rigidbody
{
public:
    Rigidbody() = default;
    ~Rigidbody() = default;

    void Update(FLOAT timeElapsed, const XMFLOAT3& globalGravity);
    void AddForce(const XMFLOAT3& force, ForceMode mode);

    weak_ptr<GameObject> GetOwner() const { return m_owner; }
    XMFLOAT3 GetVelocity() const { return velocity; }
    float GetMass() const { return mass; }
    float GetDrag() const { return drag; }
    float GetGroundFriction() const { return groundFriction; }
    float GetRestitution() const { return restitution; }
    bool IsKinematic() const { return isKinematic; }
    bool IsGrounded() const { return isGrounded; }
    CollisionDetectionMode GetCollisionDetectionMode() const { return collisionDetectionMode; }
    XMFLOAT3 GetPreviousPosition() const { return previousPosition; }
    XMFLOAT3 GetLastDisplacement() const { return lastDisplacement; }
    bool HasSweptMotion() const { return hasPreviousPosition && Utiles::Vector3::Dot(lastDisplacement, lastDisplacement) > 0.0f; }

    void SetOwner(const shared_ptr<GameObject>& owner) { m_owner = owner; }
    void SetMass(float m) { mass = max(0.001f, m); }
    void SetDrag(float d) { drag = max(0.0f, d); }
    void SetGroundFriction(float friction) { groundFriction = max(0.0f, friction); }
    void SetRestitution(float r) { restitution = clamp(r, 0.0f, 1.0f); }
    void SetKinematic(bool kinematic) { isKinematic = kinematic; }
    void SetGrounded(bool grounded) { isGrounded = grounded; }
    void SetUseGravity(bool gravity) { useGravity = gravity; }
    void SetVelocity(XMFLOAT3 v) { velocity = v; }
    void SetCollisionDetectionMode(CollisionDetectionMode mode) { collisionDetectionMode = mode; }

private:
    float mass = 1.0f;
    float drag = 1.0f;
    float groundFriction = 6.0f;
    float restitution = 0.5f;
    float gravityScale = 1.0f;
    bool isKinematic = false;
    bool useGravity = true;
    bool isGrounded = false;
    bool hasPreviousPosition = false;
    CollisionDetectionMode collisionDetectionMode = CollisionDetectionMode::ContinuousDynamic;

    XMFLOAT3 velocity{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 acceleration{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 accumulatedForce{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 previousPosition{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 lastDisplacement{ 0.0f, 0.0f, 0.0f };

    weak_ptr<GameObject> m_owner;
};
