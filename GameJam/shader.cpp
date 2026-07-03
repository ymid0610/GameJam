#include "shader.h"

namespace
{
	std::wstring GetDirectoryPath(const std::wstring& path)
	{
		size_t slash = path.find_last_of(L"\\/");
		if (slash == std::wstring::npos) return L"";
		return path.substr(0, slash + 1);
	}

	bool FileExists(const std::wstring& path)
	{
		return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	std::wstring ResolveShaderPath()
	{
		const std::wstring localShader = L"shader.hlsl";
		if (FileExists(localShader)) return localShader;

		WCHAR modulePath[MAX_PATH]{};
		DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
		if (modulePathLength == 0 || modulePathLength == _countof(modulePath)) return localShader;

		std::wstring executableDirectory = GetDirectoryPath(modulePath);
		std::wstring outputShader = executableDirectory + localShader;
		if (FileExists(outputShader)) return outputShader;

		std::wstring projectShader = executableDirectory + L"..\\..\\GameJam\\shader.hlsl";
		if (FileExists(projectShader)) return projectShader;

		projectShader = executableDirectory + L"..\\..\\01. Framework\\shader.hlsl";
		if (FileExists(projectShader)) return projectShader;

		return localShader;
	}

	void CompileShaderFromResolvedFile(const std::wstring& shaderPath, LPCSTR entryPoint, LPCSTR target,
		UINT compileFlags, ComPtr<ID3DBlob>& byteCode)
	{
		ComPtr<ID3DBlob> error;
		HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint, target, compileFlags, 0, &byteCode, &error);

		if (FAILED(hr) && error)
		{
			OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
		}

		Utiles::ThrowIfFailed(hr);
	}

	UINT AlignConstantBufferSize(UINT size)
	{
		return (size + 255) & ~255;
	}
}

Shader::Shader(const ComPtr<ID3D12Device>& device,
	const ComPtr<ID3D12RootSignature>& rootSignature,
	LPCSTR pixelShaderEntry,
	bool depthEnabled,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType,
	LPCSTR vertexShaderEntry,
	bool renderTargetEnabled,
	DXGI_FORMAT depthStencilFormat,
	INT depthBias,
	FLOAT slopeScaledDepthBias)
{
	m_lightConstants.meta = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };

#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ComPtr<ID3DBlob> mvsByteCode, mpsByteCode;
	std::wstring shaderPath = ResolveShaderPath();
	CompileShaderFromResolvedFile(shaderPath, vertexShaderEntry, "vs_5_1", compileFlags, mvsByteCode);
	if (pixelShaderEntry && pixelShaderEntry[0] != '\0')
	{
		CompileShaderFromResolvedFile(shaderPath, pixelShaderEntry, "ps_5_1", compileFlags, mpsByteCode);
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize() };
	if (mpsByteCode)
	{
		psoDesc.PS = {
			reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
			mpsByteCode->GetBufferSize() };
	}
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.DepthBias = depthBias;
	psoDesc.RasterizerState.SlopeScaledDepthBias = slopeScaledDepthBias;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = depthEnabled;
	psoDesc.DepthStencilState.DepthWriteMask = depthEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = primitiveTopologyType;
	psoDesc.NumRenderTargets = renderTargetEnabled ? 1u : 0u;
	if (renderTargetEnabled) psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = depthStencilFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	Utiles::ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

	const UINT lightBufferSize = AlignConstantBufferSize(static_cast<UINT>(sizeof(LightBufferData)));
	auto uploadHeapProperties1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resourceDesc1 = CD3DX12_RESOURCE_DESC::Buffer(lightBufferSize);
	Utiles::ThrowIfFailed(device->CreateCommittedResource(
		&uploadHeapProperties1,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc1,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_lightBuffer)));

	Utiles::ThrowIfFailed(m_lightBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedLightData)));
	memcpy(m_mappedLightData, &m_lightConstants, sizeof(m_lightConstants));

	const UINT defaultMaterialBufferSize = AlignConstantBufferSize(static_cast<UINT>(sizeof(MaterialConstants)));
	auto materialUploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto materialBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(defaultMaterialBufferSize);
	Utiles::ThrowIfFailed(device->CreateCommittedResource(
		&materialUploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&materialBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_defaultMaterialBuffer)));

	Utiles::ThrowIfFailed(m_defaultMaterialBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedDefaultMaterialData)));
	memcpy(m_mappedDefaultMaterialData, &m_defaultMaterialConstants, sizeof(m_defaultMaterialConstants));
}

Shader::~Shader()
{
	if (m_lightBuffer && m_mappedLightData)
	{
		m_lightBuffer->Unmap(0, nullptr);
		m_mappedLightData = nullptr;
	}

	if (m_defaultMaterialBuffer && m_mappedDefaultMaterialData)
	{
		m_defaultMaterialBuffer->Unmap(0, nullptr);
		m_mappedDefaultMaterialData = nullptr;
	}
}

void Shader::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	commandList->SetPipelineState(m_pipelineState.Get());
	if (m_lightBuffer) commandList->SetGraphicsRootConstantBufferView(2, m_lightBuffer->GetGPUVirtualAddress());
	if (m_defaultMaterialBuffer) commandList->SetGraphicsRootConstantBufferView(3, m_defaultMaterialBuffer->GetGPUVirtualAddress());
}

void Shader::SetLights(const vector<LightShaderData>& lightData)
{
	m_lightConstants = LightBufferData{};
	UINT lightCount = min(static_cast<UINT>(lightData.size()), Lighting::MaxLights);
	m_lightConstants.meta = XMFLOAT4{ static_cast<float>(lightCount), 0.0f, 0.0f, 0.0f };

	for (UINT i = 0; i < lightCount; ++i)
	{
		m_lightConstants.lights[i] = lightData[i];
	}

	if (m_mappedLightData) memcpy(m_mappedLightData, &m_lightConstants, sizeof(m_lightConstants));
}
