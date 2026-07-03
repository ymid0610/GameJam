#pragma once
#include "stdafx.h"
#include "light.h"

class Shader
{
public:
	Shader(const ComPtr<ID3D12Device>& device,
		const ComPtr<ID3D12RootSignature>& rootSignature,
		LPCSTR pixelShaderEntry = "PIXEL_MAIN",
		bool depthEnabled = true,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	virtual ~Shader();

	virtual void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList);
	void SetLights(const vector<LightShaderData>& lightData);

protected:
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12Resource> m_lightBuffer;
	LightBufferData m_lightConstants{};
	UINT8* m_mappedLightData = nullptr;
};
