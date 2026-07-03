#pragma once
#include "stdafx.h"
#include "rigidbody.h"

class PhysicsManager
{
public:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    void Update(FLOAT timeElapsed);
    void AddRigidbody(const shared_ptr<Rigidbody>& rigidbody);
    void RemoveRigidbody(const shared_ptr<Rigidbody>& rigidbody);

private:
    vector<shared_ptr<Rigidbody>> m_rigidbodies;
    XMFLOAT3 m_globalGravity{ 0.0f, -9.81f, 0.0f };
};
