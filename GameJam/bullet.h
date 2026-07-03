#pragma once
#include "object.h"

class Bullet final : public GameObject
{
public:
    Bullet(const shared_ptr<Mesh>& mesh, const XMFLOAT3& position, const XMFLOAT3& direction,
        float speed, float lifetime);

    void Update(FLOAT timeElapsed) override;

    bool IsExpired() const { return m_expired; }
    void MarkExpired() { m_expired = true; }

    XMFLOAT3 GetPreviousPosition() const { return m_previousPosition; }
    XMFLOAT3 GetDirection() const { return m_direction; }
    float GetLastTravelDistance() const { return m_lastTravelDistance; }

private:
    void SetTransformFromDirection(const XMFLOAT3& position);

private:
    XMFLOAT3 m_direction{ 0.0f, 0.0f, 1.0f };
    XMFLOAT3 m_previousPosition{ 0.0f, 0.0f, 0.0f };
    float m_speed = 0.0f;
    float m_lifetime = 0.0f;
    float m_age = 0.0f;
    float m_lastTravelDistance = 0.0f;
    bool m_expired = false;
};
