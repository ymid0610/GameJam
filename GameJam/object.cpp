#include "object.h"

GameObject::GameObject()
{
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixIdentity());
}

void GameObject::Update(FLOAT timeElapsed)
{
    (void)timeElapsed;
    if (m_collider) m_collider->Update(m_worldMatrix);
}

void GameObject::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    XMFLOAT4X4 worldMatrix{};
    XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(XMLoadFloat4x4(&m_worldMatrix)));
    commandList->SetGraphicsRoot32BitConstants(0, 16, &worldMatrix, 0);
}

void GameObject::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (!m_mesh) return;

    UpdateShaderVariable(commandList);
    m_mesh->Render(commandList);
}

float GameObject::GetInverseMass() const
{
    if (!m_rigidbody || m_rigidbody->IsKinematic()) return 0.0f;
    return 1.0f / m_rigidbody->GetMass();
}

void GameObject::SetWorldMatrix(const XMFLOAT4X4& worldMatrix)
{
    m_worldMatrix = worldMatrix;

    m_right = XMFLOAT3{ m_worldMatrix._11, m_worldMatrix._12, m_worldMatrix._13 };
    m_up = XMFLOAT3{ m_worldMatrix._21, m_worldMatrix._22, m_worldMatrix._23 };
    m_front = XMFLOAT3{ m_worldMatrix._31, m_worldMatrix._32, m_worldMatrix._33 };

    if (m_collider) m_collider->Update(m_worldMatrix);
}

void GameObject::SetPosition(XMFLOAT3 position)
{
    m_worldMatrix._41 = position.x;
    m_worldMatrix._42 = position.y;
    m_worldMatrix._43 = position.z;

    if (m_collider) m_collider->Update(m_worldMatrix);
}

void GameObject::SetCollider(const shared_ptr<Collider>& collider)
{
    m_collider = collider;
    if (!m_collider) return;

    m_collider->SetOwner(shared_from_this());
    m_collider->Update(m_worldMatrix);
}

void GameObject::SetRigidbody(const shared_ptr<Rigidbody>& rigidbody)
{
    m_rigidbody = rigidbody;
    if (m_rigidbody) m_rigidbody->SetOwner(shared_from_this());
}

void GameObject::Transform(XMFLOAT3 shift)
{
    SetPosition(Utiles::Vector3::Add(GetPosition(), shift));
}

void GameObject::Rotate(FLOAT pitch, FLOAT yaw, FLOAT roll)
{
    XMMATRIX rotate = XMMatrixRotationRollPitchYaw(XMConvertToRadians(pitch), XMConvertToRadians(yaw), XMConvertToRadians(roll));
    XMStoreFloat4x4(&m_worldMatrix, rotate * XMLoadFloat4x4(&m_worldMatrix));

    XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rotate));
    XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotate));
    XMStoreFloat3(&m_front, XMVector3TransformNormal(XMLoadFloat3(&m_front), rotate));

    if (m_collider) m_collider->Update(m_worldMatrix);
}

void GameObject::AddImpulse(XMFLOAT3 impulse)
{
    if (m_rigidbody && !m_rigidbody->IsKinematic())
    {
        m_rigidbody->AddForce(impulse, ForceMode::Impulse);
    }
}
