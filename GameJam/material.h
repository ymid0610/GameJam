#pragma once
#include "stdafx.h"
#include "shader.h"

class Material final
{
public:
    Material(const ComPtr<ID3D12Device>& device, shared_ptr<Shader> shader);
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) = delete;
    Material& operator=(Material&&) = delete;

    static shared_ptr<Material> Create(const ComPtr<ID3D12Device>& device,
        const shared_ptr<Shader>& shader,
        const XMFLOAT4& baseColor = XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });

    void Apply(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;

    void SetName(string name) { m_name = std::move(name); }
    const string& GetName() const { return m_name; }

    void SetShader(shared_ptr<Shader> shader) { m_shader = std::move(shader); }
    shared_ptr<Shader> GetShader() const { return m_shader; }

    void SetBaseColor(const XMFLOAT4& color);
    void SetEmission(const XMFLOAT3& color, float intensity = 1.0f);
    void SetSurface(float metallic, float roughness);
    void SetTerrainTexture(float tiling = 0.20f);

    const XMFLOAT4& GetBaseColor() const { return m_constants.baseColor; }
    const XMFLOAT4& GetEmission() const { return m_constants.emission; }
    float GetMetallic() const { return m_constants.surface.x; }
    float GetRoughness() const { return m_constants.surface.y; }
    const MaterialConstants& GetConstants() const { return m_constants; }

private:
    void UploadConstants();

private:
    string m_name;
    shared_ptr<Shader> m_shader;
    MaterialConstants m_constants{};
    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_mappedData = nullptr;
};
