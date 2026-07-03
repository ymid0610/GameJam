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

		std::wstring projectShader = executableDirectory + L"..\\..\\01. Framework\\shader.hlsl";
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
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType)
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
	CompileShaderFromResolvedFile(shaderPath, "VERTEX_MAIN", "vs_5_1", compileFlags, mvsByteCode);
	CompileShaderFromResolvedFile(shaderPath, pixelShaderEntry, "ps_5_1", compileFlags, mpsByteCode);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize() };
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = depthEnabled;
	psoDesc.DepthStencilState.DepthWriteMask = depthEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = primitiveTopologyType;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	Utiles::ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

	const UINT lightBufferSize = AlignConstantBufferSize(static_cast<UINT>(sizeof(LightBufferData)));
	Utiles::ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(lightBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_lightBuffer)));

	Utiles::ThrowIfFailed(m_lightBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedLightData)));
	memcpy(m_mappedLightData, &m_lightConstants, sizeof(m_lightConstants));
}

Shader::~Shader()
{
	if (m_lightBuffer && m_mappedLightData)
	{
		m_lightBuffer->Unmap(0, nullptr);
		m_mappedLightData = nullptr;
	}
}

void Shader::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	commandList->SetPipelineState(m_pipelineState.Get());
	if (m_lightBuffer) commandList->SetGraphicsRootConstantBufferView(2, m_lightBuffer->GetGPUVirtualAddress());
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
