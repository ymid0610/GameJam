#pragma once
#include "stdafx.h"
#include "mesh.h"
#include "material.h"
#include "component.h"
#include "collider.h"
#include "rigidbody.h"

class GameObject : public enable_shared_from_this<GameObject>
{
public:
    GameObject();
    virtual ~GameObject() = default;

    virtual void Update(FLOAT timeElapsed);
    virtual void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    virtual void RenderWithShader(const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const shared_ptr<Shader>& shader) const;
    virtual void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    virtual void OnCollisionEnter(const shared_ptr<Collider>& other) {}

    shared_ptr<Collider> GetCollider() const { return m_collider; }
    shared_ptr<Rigidbody> GetRigidbody() const { return m_rigidbody; }
    const string& GetName() const { return m_name; }
    const string& GetMeshName() const { return m_meshName; }
    XMFLOAT4X4 GetWorldMatrix() const { return m_worldMatrix; }
    XMFLOAT3 GetPosition() const { return XMFLOAT3{ m_worldMatrix._41, m_worldMatrix._42, m_worldMatrix._43 }; }
    XMFLOAT3 GetFront() const { return m_front; }
    shared_ptr<Material> GetMaterial() const { return m_material; }

    virtual float GetInverseMass() const;
    virtual float GetRestitution() const { return m_rigidbody ? m_rigidbody->GetRestitution() : 0.0f; }
    virtual XMFLOAT3 GetVelocity() const { return m_rigidbody ? m_rigidbody->GetVelocity() : XMFLOAT3{ 0.0f, 0.0f, 0.0f }; }
    virtual bool IsCharacterController() const { return false; }

    void SetMesh(const shared_ptr<Mesh>& mesh) { m_mesh = mesh; }
    void SetMaterial(const shared_ptr<Material>& material) { m_material = material; }
    void SetName(const string& name) { m_name = name; }
    void SetMeshName(const string& meshName) { m_meshName = meshName; }
    void SetWorldMatrix(const XMFLOAT4X4& worldMatrix);
    void SetPosition(XMFLOAT3 position);
    void SetCollider(const shared_ptr<Collider>& collider);
    void SetRigidbody(const shared_ptr<Rigidbody>& rigidbody);

    void Transform(XMFLOAT3 shift);
    void Rotate(FLOAT pitch, FLOAT yaw, FLOAT roll);
    virtual void AddImpulse(XMFLOAT3 impulse);

    template <typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        static_assert(is_base_of_v<Component, T>, "T must derive from Component");
        auto component = make_unique<T>(*this, std::forward<Args>(args)...);
        T& reference = *component;
        m_components.push_back(std::move(component));
        return reference;
    }

    template <typename T>
    T* GetComponent() const
    {
        static_assert(is_base_of_v<Component, T>, "T must derive from Component");
        for (const auto& component : m_components)
        {
            if (auto typed = dynamic_cast<T*>(component.get())) return typed;
        }
        return nullptr;
    }

protected:
    XMFLOAT4X4 m_worldMatrix{};
    XMFLOAT3 m_right{ 1.0f, 0.0f, 0.0f };
    XMFLOAT3 m_up{ 0.0f, 1.0f, 0.0f };
    XMFLOAT3 m_front{ 0.0f, 0.0f, 1.0f };

    shared_ptr<Mesh> m_mesh;
    shared_ptr<Material> m_material;
    shared_ptr<Collider> m_collider;
    shared_ptr<Rigidbody> m_rigidbody;
    vector<unique_ptr<Component>> m_components;
    string m_name;
    string m_meshName;
};
