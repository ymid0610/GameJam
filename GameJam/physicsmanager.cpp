#include "physicsmanager.h"

void PhysicsManager::Update(FLOAT timeElapsed)
{
    for (const auto& rigidbody : m_rigidbodies)
    {
        if (rigidbody) rigidbody->Update(timeElapsed, m_globalGravity);
    }
}

void PhysicsManager::AddRigidbody(const shared_ptr<Rigidbody>& rigidbody)
{
    if (rigidbody) m_rigidbodies.push_back(rigidbody);
}

void PhysicsManager::RemoveRigidbody(const shared_ptr<Rigidbody>& rigidbody)
{
    if (!rigidbody) return;

    erase(m_rigidbodies, rigidbody);
}
