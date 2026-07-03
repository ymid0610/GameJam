#pragma once
#include "stdafx.h"
#include "light.h"

struct MaterialConstants
{
	XMFLOAT4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT4 emission{ 0.0f, 0.0f, 0.0f, 0.0f };
	XMFLOAT4 surface{ 0.0f, 0.5f, 0.0f, 0.0f };
};

class Shader
{
public:
	Shader(const ComPtr<ID3D12Device>& device,
		const ComPtr<ID3D12RootSignature>& rootSignature,
		LPCSTR pixelShaderEntry = "PIXEL_MAIN",
		bool depthEnabled = true,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		LPCSTR vertexShaderEntry = "VERTEX_MAIN",
		bool renderTargetEnabled = true,
		DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
		INT depthBias = 0,
		FLOAT slopeScaledDepthBias = 0.0f);
	virtual ~Shader();

	virtual void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList);
	void SetLights(const vector<LightShaderData>& lightData);

protected:
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12Resource> m_lightBuffer;
	ComPtr<ID3D12Resource> m_defaultMaterialBuffer;
	LightBufferData m_lightConstants{};
	MaterialConstants m_defaultMaterialConstants{};
	UINT8* m_mappedLightData = nullptr;
	UINT8* m_mappedDefaultMaterialData = nullptr;
};
