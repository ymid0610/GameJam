#include "material.h"
#include "binaryreader.h"

namespace
{
    UINT AlignConstantBufferSize(UINT size)
    {
        return (size + 255) & ~255;
    }
}

Material::Material(const ComPtr<ID3D12Device>& device, shared_ptr<Shader> shader)
    : m_shader(std::move(shader))
{
    const UINT bufferSize = AlignConstantBufferSize(static_cast<UINT>(sizeof(MaterialConstants)));
    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    Utiles::ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    Utiles::ThrowIfFailed(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData)));
    UploadConstants();
}

Material::~Material()
{
    if (m_constantBuffer && m_mappedData)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
}

shared_ptr<Material> Material::Create(const ComPtr<ID3D12Device>& device,
    const shared_ptr<Shader>& shader,
    const XMFLOAT4& baseColor)
{
    auto material = make_shared<Material>(device, shader);
    material->SetBaseColor(baseColor);
    return material;
}

shared_ptr<Material> Material::CreateFromAsset(const ComPtr<ID3D12Device>& device,
    const shared_ptr<Shader>& shader,
    const string& filePath)
{
    auto material = make_shared<Material>(device, shader);
    BinaryReader reader{ filesystem::path(filePath) };

    while (!reader.End())
    {
        string token = reader.ReadString();
        if (token == "<Name>:")
        {
            material->SetName(reader.ReadString());
        }
        else if (token == "<BaseColor>:")
        {
            material->SetBaseColor(reader.Read<XMFLOAT4>());
        }
        else if (token == "<Emission>:")
        {
            XMFLOAT4 emission = reader.Read<XMFLOAT4>();
            material->SetEmission(XMFLOAT3{ emission.x, emission.y, emission.z }, emission.w);
        }
        else if (token == "<Surface>:")
        {
            material->m_constants.surface = reader.Read<XMFLOAT4>();
            material->UploadConstants();
        }
        else if (token == "<AlbedoTexture>:")
        {
            material->SetAlbedoTexturePath(reader.ReadString());
        }
        else if (token == "<UseVertexColors>:")
        {
            const uint8_t enabled = reader.Read<uint8_t>();
            if (enabled != 0) material->SetVertexColorAlbedo();
        }
        else if (token == "</Material>")
        {
            break;
        }
        else
        {
            throw runtime_error("Unknown material asset token: " + token);
        }
    }

    return material;
}

void Material::Apply(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (m_shader) m_shader->UpdateShaderVariable(commandList);
    if (m_constantBuffer)
    {
        commandList->SetGraphicsRootConstantBufferView(3, m_constantBuffer->GetGPUVirtualAddress());
    }
}

void Material::SetBaseColor(const XMFLOAT4& color)
{
    m_constants.baseColor = color;
    UploadConstants();
}

void Material::SetEmission(const XMFLOAT3& color, float intensity)
{
    m_constants.emission = XMFLOAT4{ color.x, color.y, color.z, max(0.0f, intensity) };
    UploadConstants();
}

void Material::SetSurface(float metallic, float roughness)
{
    m_constants.surface.x = clamp(metallic, 0.0f, 1.0f);
    m_constants.surface.y = clamp(roughness, 0.0f, 1.0f);
    UploadConstants();
}

void Material::SetTerrainTexture(float tiling)
{
    m_constants.surface.z = 1.0f;
    m_constants.surface.w = max(0.01f, tiling);
    UploadConstants();
}

void Material::SetVertexColorAlbedo()
{
    m_constants.surface.z = 2.0f;
    UploadConstants();
}

void Material::UploadConstants()
{
    if (m_mappedData) memcpy(m_mappedData, &m_constants, sizeof(m_constants));
}
