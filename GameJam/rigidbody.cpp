#include "rigidbody.h"
#include "object.h"

void Rigidbody::Update(FLOAT timeElapsed, const XMFLOAT3& globalGravity)
{
    if (isKinematic) {
        velocity = { 0.f, 0.f, 0.f };
        accumulatedForce = { 0.f, 0.f, 0.f };
        lastDisplacement = { 0.f, 0.f, 0.f };
        return;
    }

    if (isGrounded && velocity.y < 0.0f) {
        velocity.y = 0.0f;
    }

    // 중력을 누적 힘에 합산 
    if (useGravity && !(isGrounded && velocity.y <= 0.0f)) {
        accumulatedForce.x += globalGravity.x * gravityScale;
        accumulatedForce.y += globalGravity.y * gravityScale;
        accumulatedForce.z += globalGravity.z * gravityScale;
    }

    // 가속도(a) = 누적된 힘(F). 이미 AddForce에서 질량을 나눴으므로 그대로 씁니다.
    acceleration = accumulatedForce;

    // 속도(Velocity) 적분: v = v0 + at
    velocity.x += acceleration.x * timeElapsed;
    velocity.y += acceleration.y * timeElapsed;
    velocity.z += acceleration.z * timeElapsed;

    // 드래그(마찰/공기저항) 적용: 속도가 서서히 줄어들게 만듦
    // 공식: v = v * (1 - drag * dt)
    float dragFactor = max(0.0f, 1.0f - (drag * timeElapsed));
    velocity.x *= dragFactor;
    velocity.y *= dragFactor;
    velocity.z *= dragFactor;

    if (isGrounded && groundFriction > 0.0f) {
        float groundFactor = max(0.0f, 1.0f - (groundFriction * timeElapsed));
        velocity.x *= groundFactor;
        velocity.z *= groundFactor;

        float horizontalSpeedSq = velocity.x * velocity.x + velocity.z * velocity.z;
        if (horizontalSpeedSq <= 0.0001f) {
            velocity.x = 0.0f;
            velocity.z = 0.0f;
        }
    }
    // 위치(Position) 적분: p = p0 + vt
    DirectX::XMFLOAT3 displacement = Utiles::Vector3::Mul(velocity, timeElapsed);

    if (auto owner = m_owner.lock())
    {
        previousPosition = owner->GetPosition();
        owner->Transform(displacement);
        lastDisplacement = displacement;
        hasPreviousPosition = true;
    }
    else
    {
        lastDisplacement = { 0.f, 0.f, 0.f };
    }

    // 한 프레임 처리가 끝났으므로 지속 힘(Force) 초기화
    accumulatedForce = { 0.f, 0.f, 0.f };

    isGrounded = false;
}

void Rigidbody::AddForce(const DirectX::XMFLOAT3& force, ForceMode mode)
{
    switch (mode)
    {
    case ForceMode::Force: // F = ma -> a = F/m (매 프레임 지속되는 힘)
        accumulatedForce.x += force.x / mass;
        accumulatedForce.y += force.y / mass;
        accumulatedForce.z += force.z / mass;
        break;

    case ForceMode::Impulse: // 즉각적인 충격 (프레임 시간과 무관하게 한 번 작용)
        // 충격량 I = m * dv -> dv = I / m
        velocity.x += force.x / mass;
        velocity.y += force.y / mass;
        velocity.z += force.z / mass;
        break;

    case ForceMode::VelocityChange: // 질량 무시 즉시 속도 변화
        velocity.x += force.x;
        velocity.y += force.y;
        velocity.z += force.z;
        break;
    }
}
