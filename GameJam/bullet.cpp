#include "bullet.h"

namespace
{
    XMFLOAT3 SafeNormalize(const XMFLOAT3& value, const XMFLOAT3& fallback)
    {
        float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
        if (lengthSq <= Utiles::Physics::Epsilon) return fallback;

        float invLength = 1.0f / sqrtf(lengthSq);
        return XMFLOAT3{ value.x * invLength, value.y * invLength, value.z * invLength };
    }
}

Bullet::Bullet(const shared_ptr<Mesh>& mesh, const XMFLOAT3& position, const XMFLOAT3& direction,
    float speed, float lifetime)
    : m_direction{ SafeNormalize(direction, XMFLOAT3{ 0.0f, 0.0f, 1.0f }) },
    m_previousPosition{ position },
    m_speed{ speed },
    m_lifetime{ lifetime }
{
    SetMesh(mesh);
    SetTransformFromDirection(position);
}

void Bullet::Update(FLOAT timeElapsed)
{
    if (m_expired) return;

    m_previousPosition = GetPosition();
    m_lastTravelDistance = max(0.0f, m_speed * timeElapsed);

    XMFLOAT3 displacement = Utiles::Vector3::Mul(m_direction, m_lastTravelDistance);
    SetPosition(Utiles::Vector3::Add(m_previousPosition, displacement));

    m_age += timeElapsed;
    if (m_age >= m_lifetime) m_expired = true;
}

void Bullet::SetTransformFromDirection(const XMFLOAT3& position)
{
    XMFLOAT3 forward = m_direction;
    XMFLOAT3 worldUp{ 0.0f, 1.0f, 0.0f };
    XMFLOAT3 right = Utiles::Vector3::Cross(worldUp, forward);
    right = SafeNormalize(right, XMFLOAT3{ 1.0f, 0.0f, 0.0f });
    XMFLOAT3 up = SafeNormalize(Utiles::Vector3::Cross(forward, right), worldUp);

    SetWorldMatrix(XMFLOAT4X4{
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        position.x, position.y, position.z, 1.0f
    });
}
