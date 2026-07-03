#pragma once
#include "stdafx.h"
#include "shader.h"

class GameObject;

struct ShadowConstants
{
    XMFLOAT4X4 lightViewProjection{};
    XMFLOAT4 data{ 0.0f, 0.0035f, 0.48f, 1.0f / 2048.0f };
};

struct ShadowCameraConstants
{
    XMFLOAT4X4 view{};
    XMFLOAT4X4 projection{};
    XMFLOAT4 position{ 0.0f, 0.0f, 0.0f, 1.0f };
};

class ShadowMap final
{
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) = delete;
    ShadowMap& operator=(ShadowMap&&) = delete;

    void Initialize(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12RootSignature>& rootSignature,
        UINT resolution = 2048);
    void SetLight(const XMFLOAT3& lightPosition,
        const XMFLOAT3& focus,
        float coverageRadius,
        float visibility);
    void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const vector<shared_ptr<GameObject>>& objects);
    void BindForMainPass(const ComPtr<ID3D12GraphicsCommandList>& commandList);

private:
    void UploadConstants();
    void TransitionTo(const ComPtr<ID3D12GraphicsCommandList>& commandList,
        D3D12_RESOURCE_STATES nextState);

private:
    static constexpr DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_R32_TYPELESS;
    static constexpr DXGI_FORMAT ShadowDepthFormat = DXGI_FORMAT_D32_FLOAT;
    static constexpr DXGI_FORMAT ShadowSrvFormat = DXGI_FORMAT_R32_FLOAT;

    ComPtr<ID3D12Device> m_device;
    shared_ptr<Shader> m_depthShader;
    ComPtr<ID3D12Resource> m_shadowMap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12Resource> m_shadowConstantBuffer;
    UINT8* m_mappedShadowConstants = nullptr;
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{};
    D3D12_RESOURCE_STATES m_resourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    ShadowConstants m_shadowConstants{};
    ShadowCameraConstants m_shadowCameraConstants{};
    UINT m_resolution = 2048;
};
