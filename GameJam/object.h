#pragma once
#include "stdafx.h"
#include "mesh.h"
#include "collider.h"
#include "rigidbody.h"

class GameObject : public enable_shared_from_this<GameObject>
{
public:
    GameObject();
    virtual ~GameObject() = default;

    virtual void Update(FLOAT timeElapsed);
    virtual void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    virtual void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    virtual void OnCollisionEnter(const shared_ptr<Collider>& other) {}

    shared_ptr<Collider> GetCollider() const { return m_collider; }
    shared_ptr<Rigidbody> GetRigidbody() const { return m_rigidbody; }
    const string& GetName() const { return m_name; }
    const string& GetMeshName() const { return m_meshName; }
    XMFLOAT4X4 GetWorldMatrix() const { return m_worldMatrix; }
    XMFLOAT3 GetPosition() const { return XMFLOAT3{ m_worldMatrix._41, m_worldMatrix._42, m_worldMatrix._43 }; }
    XMFLOAT3 GetFront() const { return m_front; }

    virtual float GetInverseMass() const;
    virtual float GetRestitution() const { return m_rigidbody ? m_rigidbody->GetRestitution() : 0.0f; }
    virtual XMFLOAT3 GetVelocity() const { return m_rigidbody ? m_rigidbody->GetVelocity() : XMFLOAT3{ 0.0f, 0.0f, 0.0f }; }
    virtual bool IsCharacterController() const { return false; }

    void SetMesh(const shared_ptr<Mesh>& mesh) { m_mesh = mesh; }
    void SetName(const string& name) { m_name = name; }
    void SetMeshName(const string& meshName) { m_meshName = meshName; }
    void SetWorldMatrix(const XMFLOAT4X4& worldMatrix);
    void SetPosition(XMFLOAT3 position);
    void SetCollider(const shared_ptr<Collider>& collider);
    void SetRigidbody(const shared_ptr<Rigidbody>& rigidbody);

    void Transform(XMFLOAT3 shift);
    void Rotate(FLOAT pitch, FLOAT yaw, FLOAT roll);
    virtual void AddImpulse(XMFLOAT3 impulse);

protected:
    XMFLOAT4X4 m_worldMatrix{};
    XMFLOAT3 m_right{ 1.0f, 0.0f, 0.0f };
    XMFLOAT3 m_up{ 0.0f, 1.0f, 0.0f };
    XMFLOAT3 m_front{ 0.0f, 0.0f, 1.0f };

    shared_ptr<Mesh> m_mesh;
    shared_ptr<Collider> m_collider;
    shared_ptr<Rigidbody> m_rigidbody;
    string m_name;
    string m_meshName;
};
