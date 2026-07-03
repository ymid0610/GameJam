#include "shadow.h"
#include "object.h"

namespace
{
    UINT AlignConstantBufferSize(UINT size)
    {
        return (size + 255) & ~255;
    }
}

ShadowMap::~ShadowMap()
{
    if (m_shadowConstantBuffer && m_mappedShadowConstants)
    {
        m_shadowConstantBuffer->Unmap(0, nullptr);
        m_mappedShadowConstants = nullptr;
    }
}

void ShadowMap::Initialize(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12RootSignature>& rootSignature,
    UINT resolution)
{
    m_device = device;
    m_resolution = max(512u, resolution);

    m_depthShader = make_shared<Shader>(device,
        rootSignature,
        "",
        true,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        "VERTEX_MAIN",
        false,
        ShadowDepthFormat,
        12000,
        2.25f);

    D3D12_RESOURCE_DESC shadowDesc{};
    shadowDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    shadowDesc.Alignment = 0;
    shadowDesc.Width = m_resolution;
    shadowDesc.Height = m_resolution;
    shadowDesc.DepthOrArraySize = 1;
    shadowDesc.MipLevels = 1;
    shadowDesc.Format = ShadowMapFormat;
    shadowDesc.SampleDesc.Count = 1;
    shadowDesc.SampleDesc.Quality = 0;
    shadowDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    shadowDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = ShadowDepthFormat;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    Utiles::ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &shadowDesc,
        m_resourceState,
        &clearValue,
        IID_PPV_ARGS(&m_shadowMap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Utiles::ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = ShadowDepthFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    device->CreateDepthStencilView(m_shadowMap.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Utiles::ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = ShadowSrvFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(m_shadowMap.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());

    const UINT shadowConstantBufferSize = AlignConstantBufferSize(static_cast<UINT>(sizeof(ShadowConstants)));
    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(shadowConstantBufferSize);
    Utiles::ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_shadowConstantBuffer)));

    Utiles::ThrowIfFailed(m_shadowConstantBuffer->Map(0, nullptr,
        reinterpret_cast<void**>(&m_mappedShadowConstants)));

    m_viewport = D3D12_VIEWPORT{
        0.0f,
        0.0f,
        static_cast<float>(m_resolution),
        static_cast<float>(m_resolution),
        0.0f,
        1.0f
    };
    m_scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(m_resolution), static_cast<LONG>(m_resolution) };

    SetLight(XMFLOAT3{ 0.0f, 68.0f, 0.0f }, XMFLOAT3{ 0.0f, 0.0f, 0.0f }, 76.0f, 1.0f);
}

void ShadowMap::SetLight(const XMFLOAT3& lightPosition,
    const XMFLOAT3& focus,
    float coverageRadius,
    float visibility)
{
    coverageRadius = max(8.0f, coverageRadius);
    visibility = clamp(visibility, 0.0f, 1.0f);

    XMVECTOR eye = XMLoadFloat3(&lightPosition);
    XMVECTOR target = XMLoadFloat3(&focus);
    XMVECTOR lightVector = XMVectorSubtract(target, eye);
    if (XMVectorGetX(XMVector3LengthSq(lightVector)) <= 0.0001f)
    {
        eye = XMVectorSet(0.0f, coverageRadius, -coverageRadius, 1.0f);
        lightVector = XMVectorSubtract(target, eye);
    }

    XMVECTOR direction = XMVector3Normalize(lightVector);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    if (fabsf(XMVectorGetX(XMVector3Dot(direction, up))) > 0.92f)
    {
        up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }

    const float distance = max(XMVectorGetX(XMVector3Length(lightVector)), coverageRadius);
    XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
    XMMATRIX projection = XMMatrixOrthographicLH(coverageRadius * 2.0f, coverageRadius * 2.0f,
        0.5f, distance + coverageRadius * 2.2f);

    XMFLOAT4X4 viewMatrix{};
    XMFLOAT4X4 projectionMatrix{};
    XMFLOAT4X4 lightViewProjection{};
    XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(view));
    XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(projection));
    XMStoreFloat4x4(&lightViewProjection, XMMatrixTranspose(view * projection));

    m_shadowCameraConstants.view = viewMatrix;
    m_shadowCameraConstants.projection = projectionMatrix;
    XMStoreFloat4(&m_shadowCameraConstants.position, XMVectorSetW(eye, 1.0f));

    m_shadowConstants.lightViewProjection = lightViewProjection;
    m_shadowConstants.data = XMFLOAT4{
        visibility > 0.06f ? 1.0f : 0.0f,
        0.0042f,
        0.44f,
        1.0f / static_cast<float>(m_resolution)
    };
    UploadConstants();
}

void ShadowMap::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const vector<shared_ptr<GameObject>>& objects)
{
    if (!m_shadowMap || !m_depthShader) return;

    TransitionTo(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    commandList->OMSetRenderTargets(0, nullptr, false, &dsv);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->SetGraphicsRoot32BitConstants(1, 36, &m_shadowCameraConstants, 0);

    for (const auto& object : objects)
    {
        if (!object) continue;

        const auto* shadowCaster = object->GetComponent<ShadowCasterComponent>();
        if (!shadowCaster || !shadowCaster->CastsShadow()) continue;

        object->RenderWithShader(commandList, m_depthShader);
    }
}

void ShadowMap::BindForMainPass(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    if (!m_shadowMap || !m_shadowConstantBuffer || !m_srvHeap) return;

    TransitionTo(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootConstantBufferView(4, m_shadowConstantBuffer->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(5, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
}

void ShadowMap::UploadConstants()
{
    if (m_mappedShadowConstants)
    {
        memcpy(m_mappedShadowConstants, &m_shadowConstants, sizeof(m_shadowConstants));
    }
}

void ShadowMap::TransitionTo(const ComPtr<ID3D12GraphicsCommandList>& commandList,
    D3D12_RESOURCE_STATES nextState)
{
    if (!m_shadowMap || m_resourceState == nextState) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_shadowMap.Get(),
        m_resourceState,
        nextState);
    commandList->ResourceBarrier(1, &barrier);
    m_resourceState = nextState;
}
