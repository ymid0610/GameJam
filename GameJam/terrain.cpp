#include "terrain.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

namespace
{
    filesystem::path GetModuleDirectoryForTerrainLoading()
    {
        WCHAR modulePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
        if (length == 0 || length == _countof(modulePath)) return {};

        return filesystem::path(modulePath).parent_path();
    }

    filesystem::path ResolveTerrainPath(const filesystem::path& requested)
    {
        if (requested.is_absolute() && filesystem::exists(requested)) return requested;
        if (filesystem::exists(requested)) return filesystem::absolute(requested);

        vector<filesystem::path> roots;
        roots.push_back(filesystem::current_path());

        filesystem::path moduleDirectory = GetModuleDirectoryForTerrainLoading();
        if (!moduleDirectory.empty()) roots.push_back(moduleDirectory);

        size_t rootCount = roots.size();
        for (size_t i = 0; i < rootCount; ++i)
        {
            filesystem::path root = roots[i];
            for (int depth = 0; depth < 4 && !root.empty(); ++depth)
            {
                roots.push_back(root);
                root = root.parent_path();
            }
        }

        for (const auto& root : roots)
        {
            filesystem::path candidate = root / requested;
            if (filesystem::exists(candidate)) return filesystem::absolute(candidate);

            candidate = root / "01. Framework" / requested;
            if (filesystem::exists(candidate)) return filesystem::absolute(candidate);
        }

        return {};
    }

    void ThrowIfFailedWithMessage(HRESULT hr, const string& message)
    {
        if (FAILED(hr)) throw runtime_error(message);
    }

    class ScopedComInitialization
    {
    public:
        explicit ScopedComInitialization(DWORD flags)
        {
            m_result = CoInitializeEx(nullptr, flags);
            m_shouldUninitialize = SUCCEEDED(m_result);
        }

        ~ScopedComInitialization()
        {
            if (m_shouldUninitialize) CoUninitialize();
        }

        ScopedComInitialization(const ScopedComInitialization&) = delete;
        ScopedComInitialization& operator=(const ScopedComInitialization&) = delete;

        HRESULT GetResult() const { return m_result; }

    private:
        HRESULT m_result = S_OK;
        bool m_shouldUninitialize = false;
    };

    float GetLuminance(unsigned char r, unsigned char g, unsigned char b)
    {
        return (0.2126f * static_cast<float>(r) +
            0.7152f * static_cast<float>(g) +
            0.0722f * static_cast<float>(b)) / 255.0f;
    }

    template <typename Vertex>
    void CreateTerrainVertexBuffer(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const vector<Vertex>& vertices,
        ComPtr<ID3D12Resource>& vertexBuffer,
        ComPtr<ID3D12Resource>& vertexUploadBuffer,
        D3D12_VERTEX_BUFFER_VIEW& vertexBufferView)
    {
        const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));

        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer)));

        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexUploadBuffer)));

        D3D12_SUBRESOURCE_DATA vertexData{};
        vertexData.pData = vertices.data();
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;
        UpdateSubresources<1>(commandList.Get(), vertexBuffer.Get(), vertexUploadBuffer.Get(), 0, 0, 1, &vertexData);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexBufferView.SizeInBytes = vertexBufferSize;
        vertexBufferView.StrideInBytes = sizeof(Vertex);
    }

    void CreateTerrainIndexBuffer(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const vector<UINT>& indices,
        ComPtr<ID3D12Resource>& indexBuffer,
        ComPtr<ID3D12Resource>& indexUploadBuffer,
        D3D12_INDEX_BUFFER_VIEW& indexBufferView)
    {
        const UINT indexBufferSize = static_cast<UINT>(indices.size() * sizeof(UINT));

        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&indexBuffer)));

        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexUploadBuffer)));

        D3D12_SUBRESOURCE_DATA indexData{};
        indexData.pData = indices.data();
        indexData.RowPitch = indexBufferSize;
        indexData.SlicePitch = indexData.RowPitch;
        UpdateSubresources<1>(commandList.Get(), indexBuffer.Get(), indexUploadBuffer.Get(), 0, 0, 1, &indexData);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER));

        indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        indexBufferView.SizeInBytes = indexBufferSize;
    }

    XMFLOAT3 TransformPoint(const XMFLOAT3& point, CXMMATRIX matrix)
    {
        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVector3TransformCoord(XMLoadFloat3(&point), matrix));
        return result;
    }

    XMFLOAT3 TransformNormal(const XMFLOAT3& normal, CXMMATRIX matrix)
    {
        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&normal), matrix)));
        return result;
    }

    XMFLOAT4 GetTerrainColor(float height, const BoundingBox& bounds)
    {
        float minY = bounds.Center.y - bounds.Extents.y;
        float maxY = bounds.Center.y + bounds.Extents.y;
        float t = (maxY > minY) ? clamp((height - minY) / (maxY - minY), 0.0f, 1.0f) : 0.0f;

        XMFLOAT3 low{ 0.20f, 0.43f, 0.18f };
        XMFLOAT3 high{ 0.58f, 0.55f, 0.48f };
        return XMFLOAT4{
            low.x + (high.x - low.x) * t,
            low.y + (high.y - low.y) * t,
            low.z + (high.z - low.z) * t,
            1.0f
        };
    }
}

TerrainHeightMap::TerrainHeightMap(UINT width, UINT length, float cellSpacing, vector<float> heights)
    : m_width(max(width, 2u)),
    m_length(max(length, 2u)),
    m_cellSpacing(max(cellSpacing, 0.001f)),
    m_heights(std::move(heights))
{
    size_t expected = static_cast<size_t>(m_width) * m_length;
    if (m_heights.size() != expected) m_heights.assign(expected, 0.0f);
}

shared_ptr<TerrainHeightMap> TerrainHeightMap::CreateProcedural(UINT width, UINT length, float cellSpacing, float maxHeight)
{
    width = max(width, 2u);
    length = max(length, 2u);

    vector<float> heights(static_cast<size_t>(width) * length);
    for (UINT z = 0; z < length; ++z)
    {
        for (UINT x = 0; x < width; ++x)
        {
            float nx = static_cast<float>(x) / static_cast<float>(width - 1);
            float nz = static_cast<float>(z) / static_cast<float>(length - 1);
            float wave = sinf(nx * XM_2PI * 3.0f) * 0.35f + cosf(nz * XM_2PI * 2.0f) * 0.25f;
            float ridge = sinf((nx + nz) * XM_2PI * 1.7f) * 0.20f;
            float basin = -0.35f * expf(-18.0f * ((nx - 0.5f) * (nx - 0.5f) + (nz - 0.55f) * (nz - 0.55f)));

            heights[static_cast<size_t>(z) * width + x] = (wave + ridge + basin) * maxHeight;
        }
    }

    return make_shared<TerrainHeightMap>(width, length, cellSpacing, std::move(heights));
}

shared_ptr<TerrainHeightMap> TerrainHeightMap::CreateWaveField(UINT width, UINT length, float cellSpacing, float amplitude, float frequency)
{
    width = max(width, 2u);
    length = max(length, 2u);
    amplitude = max(amplitude, 0.0f);
    frequency = max(frequency, 0.1f);

    vector<float> heights(static_cast<size_t>(width) * length);
    for (UINT z = 0; z < length; ++z)
    {
        for (UINT x = 0; x < width; ++x)
        {
            float nx = static_cast<float>(x) / static_cast<float>(width - 1);
            float nz = static_cast<float>(z) / static_cast<float>(length - 1);
            float crossWave = sinf(nx * XM_2PI * frequency) * cosf(nz * XM_2PI * frequency * 0.85f);
            float diagonalWave = sinf((nx + nz) * XM_2PI * frequency * 0.55f);

            float canyonCenter = 0.5f + sinf(nx * XM_2PI * 2.0f) * 0.08f;
            float canyonOffset = nz - canyonCenter;
            float narrowCanyon = -1.35f * expf(-(canyonOffset * canyonOffset) / 0.00055f);

            heights[static_cast<size_t>(z) * width + x] =
                (crossWave * 0.65f + diagonalWave * 0.25f + narrowCanyon) * amplitude;
        }
    }

    return make_shared<TerrainHeightMap>(width, length, cellSpacing, std::move(heights));
}

shared_ptr<TerrainHeightMap> TerrainHeightMap::CreateSlopeTestField(UINT width, UINT length, float cellSpacing, const vector<float>& slopeAnglesDegrees)
{
    width = max(width, 2u);
    length = max(length, 2u);

    vector<float> slopes = slopeAnglesDegrees;
    if (slopes.empty()) slopes = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };

    float worldWidth = static_cast<float>(width - 1) * cellSpacing;
    float worldLength = static_cast<float>(length - 1) * cellSpacing;
    float laneWidth = worldWidth / static_cast<float>(slopes.size());
    float rampStart = worldLength * 0.18f;
    float rampRun = min(worldLength * 0.34f, 24.0f);

    vector<float> heights(static_cast<size_t>(width) * length);
    for (UINT z = 0; z < length; ++z)
    {
        float localZ = static_cast<float>(z) * cellSpacing;
        float rampDistance = clamp(localZ - rampStart, 0.0f, rampRun);

        for (UINT x = 0; x < width; ++x)
        {
            float localX = static_cast<float>(x) * cellSpacing;
            size_t laneIndex = min(static_cast<size_t>(localX / laneWidth), slopes.size() - 1);
            float laneLocalX = localX - laneWidth * static_cast<float>(laneIndex);
            float laneEdgeDistance = min(laneLocalX, laneWidth - laneLocalX);
            float laneBlend = clamp((laneEdgeDistance - laneWidth * 0.04f) / (laneWidth * 0.08f), 0.0f, 1.0f);

            float angle = clamp(slopes[laneIndex], 0.0f, 75.0f);
            float slope = tanf(XMConvertToRadians(angle));
            float height = rampDistance * slope;

            heights[static_cast<size_t>(z) * width + x] = height * laneBlend;
        }
    }

    return make_shared<TerrainHeightMap>(width, length, cellSpacing, std::move(heights));
}

shared_ptr<TerrainHeightMap> TerrainHeightMap::LoadRaw8(const string& filePath, UINT width, UINT length, float cellSpacing, float heightScale)
{
    ifstream file(filePath, ios::binary);
    if (!file) throw runtime_error("Failed to open terrain height map: " + filePath);

    vector<unsigned char> bytes(static_cast<size_t>(width) * length);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
    if (!file) throw runtime_error("Invalid terrain height map size: " + filePath);

    vector<float> heights(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        heights[i] = (static_cast<float>(bytes[i]) / 255.0f) * heightScale;
    }

    return make_shared<TerrainHeightMap>(width, length, cellSpacing, std::move(heights));
}

shared_ptr<TerrainHeightMap> TerrainHeightMap::LoadRawAuto(const string& filePath, float cellSpacing, float heightScale)
{
    filesystem::path resolvedPath = ResolveTerrainPath(filePath);
    if (resolvedPath.empty()) throw runtime_error("Failed to find terrain RAW height map: " + filePath);

    ifstream file(resolvedPath, ios::binary | ios::ate);
    if (!file) throw runtime_error("Failed to open terrain RAW height map: " + resolvedPath.string());

    streamoff fileSize = file.tellg();
    if (fileSize <= 0) throw runtime_error("Terrain RAW height map is empty: " + resolvedPath.string());

    auto inferSquareDimension = [](uint64_t sampleCount) -> UINT
    {
        double root = sqrt(static_cast<double>(sampleCount));
        uint64_t dimension = static_cast<uint64_t>(root + 0.5);
        constexpr uint64_t MaxUintValue = 0xffffffffu;
        if (dimension >= 2 && dimension * dimension == sampleCount && dimension <= MaxUintValue)
        {
            return static_cast<UINT>(dimension);
        }

        return 0;
    };

    uint64_t byteCount = static_cast<uint64_t>(fileSize);
    UINT dimension8 = inferSquareDimension(byteCount);
    UINT dimension16 = (byteCount % 2 == 0) ? inferSquareDimension(byteCount / 2) : 0;
    bool isRaw16 = dimension8 == 0 && dimension16 != 0;
    UINT dimension = isRaw16 ? dimension16 : dimension8;
    if (dimension == 0)
    {
        throw runtime_error("RAW height map size must be square 8-bit or 16-bit data: " + resolvedPath.string());
    }

    file.seekg(0, ios::beg);
    vector<unsigned char> bytes(static_cast<size_t>(byteCount));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
    if (!file) throw runtime_error("Failed to read terrain RAW height map: " + resolvedPath.string());

    heightScale = max(heightScale, 0.0f);
    size_t sampleCount = static_cast<size_t>(dimension) * dimension;
    vector<float> heights(sampleCount);
    if (isRaw16)
    {
        for (size_t i = 0; i < sampleCount; ++i)
        {
            unsigned int lo = static_cast<unsigned int>(bytes[i * 2 + 0]);
            unsigned int hi = static_cast<unsigned int>(bytes[i * 2 + 1]);
            unsigned int value = lo | (hi << 8);
            heights[i] = (static_cast<float>(value) / 65535.0f) * heightScale;
        }
    }
    else
    {
        for (size_t i = 0; i < sampleCount; ++i)
        {
            heights[i] = (static_cast<float>(bytes[i]) / 255.0f) * heightScale;
        }
    }

    return make_shared<TerrainHeightMap>(dimension, dimension, cellSpacing, std::move(heights));
}

shared_ptr<TerrainHeightMap> TerrainHeightMap::LoadImage8(const string& filePath, float cellSpacing, float heightScale)
{
    filesystem::path resolvedPath = ResolveTerrainPath(filePath);
    if (resolvedPath.empty()) throw runtime_error("Failed to find terrain height map image: " + filePath);

    ScopedComInitialization comInitialization(COINIT_MULTITHREADED);

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    ThrowIfFailedWithMessage(hr, "Failed to create WIC imaging factory.");

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        resolvedPath.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    ThrowIfFailedWithMessage(hr, "Failed to open terrain height map image: " + resolvedPath.string());

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    ThrowIfFailedWithMessage(hr, "Failed to read terrain height map image frame.");

    UINT width = 0;
    UINT length = 0;
    hr = frame->GetSize(&width, &length);
    ThrowIfFailedWithMessage(hr, "Failed to read terrain height map image size.");
    if (width < 2 || length < 2) throw runtime_error("Terrain height map image is too small: " + resolvedPath.string());

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    ThrowIfFailedWithMessage(hr, "Failed to create terrain height map image converter.");

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom);
    ThrowIfFailedWithMessage(hr, "Failed to convert terrain height map image to RGBA.");

    const UINT bytesPerPixel = 4;
    const UINT rowPitch = width * bytesPerPixel;
    vector<unsigned char> pixels(static_cast<size_t>(rowPitch) * length);
    hr = converter->CopyPixels(nullptr, rowPitch, static_cast<UINT>(pixels.size()), pixels.data());
    ThrowIfFailedWithMessage(hr, "Failed to copy terrain height map image pixels.");

    heightScale = max(heightScale, 0.0f);
    vector<float> heights(static_cast<size_t>(width) * length);
    for (UINT z = 0; z < length; ++z)
    {
        const unsigned char* row = pixels.data() + static_cast<size_t>(z) * rowPitch;
        for (UINT x = 0; x < width; ++x)
        {
            const unsigned char* pixel = row + static_cast<size_t>(x) * bytesPerPixel;
            float luminance = GetLuminance(pixel[0], pixel[1], pixel[2]);
            heights[static_cast<size_t>(z) * width + x] = luminance * heightScale;
        }
    }

    return make_shared<TerrainHeightMap>(width, length, cellSpacing, std::move(heights));
}

bool TerrainHeightMap::ContainsLocalXZ(float x, float z) const
{
    return x >= 0.0f && z >= 0.0f && x <= GetWorldWidth() && z <= GetWorldLength();
}

float TerrainHeightMap::SampleHeight(float x, float z) const
{
    if (m_heights.empty()) return 0.0f;

    x = clamp(x, 0.0f, GetWorldWidth());
    z = clamp(z, 0.0f, GetWorldLength());

    float gridX = x / m_cellSpacing;
    float gridZ = z / m_cellSpacing;

    UINT x0 = min(static_cast<UINT>(floorf(gridX)), m_width - 1);
    UINT z0 = min(static_cast<UINT>(floorf(gridZ)), m_length - 1);
    UINT x1 = min(x0 + 1, m_width - 1);
    UINT z1 = min(z0 + 1, m_length - 1);

    float tx = gridX - static_cast<float>(x0);
    float tz = gridZ - static_cast<float>(z0);

    float h00 = HeightAt(x0, z0);
    float h10 = HeightAt(x1, z0);
    float h01 = HeightAt(x0, z1);
    float h11 = HeightAt(x1, z1);

    float h0 = h00 + (h10 - h00) * tx;
    float h1 = h01 + (h11 - h01) * tx;
    return h0 + (h1 - h0) * tz;
}

XMFLOAT3 TerrainHeightMap::SampleNormal(float x, float z) const
{
    float left = SampleHeight(x - m_cellSpacing, z);
    float right = SampleHeight(x + m_cellSpacing, z);
    float back = SampleHeight(x, z - m_cellSpacing);
    float front = SampleHeight(x, z + m_cellSpacing);

    XMFLOAT3 normal{ left - right, 2.0f * m_cellSpacing, back - front };
    return Utiles::Vector3::Normalize(normal);
}

BoundingBox TerrainHeightMap::GetLocalAABB() const
{
    auto [minIt, maxIt] = minmax_element(m_heights.begin(), m_heights.end());
    float minHeight = minIt == m_heights.end() ? 0.0f : *minIt;
    float maxHeight = maxIt == m_heights.end() ? 0.0f : *maxIt;

    XMFLOAT3 minPoint{ 0.0f, minHeight, 0.0f };
    XMFLOAT3 maxPoint{ GetWorldWidth(), maxHeight, GetWorldLength() };
    BoundingBox box{};
    BoundingBox::CreateFromPoints(box, XMLoadFloat3(&minPoint), XMLoadFloat3(&maxPoint));
    return box;
}

float TerrainHeightMap::HeightAt(UINT x, UINT z) const
{
    x = min(x, m_width - 1);
    z = min(z, m_length - 1);
    return m_heights[Index(x, z)];
}

TerrainMesh::TerrainMesh(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const shared_ptr<const TerrainHeightMap>& heightMap)
{
    if (!heightMap) throw runtime_error("TerrainMesh requires a height map.");

    vector<Vertex> vertices;
    vector<UINT> indices;
    vertices.reserve(static_cast<size_t>(heightMap->GetWidth()) * heightMap->GetLength());
    indices.reserve(static_cast<size_t>(heightMap->GetWidth() - 1) * (heightMap->GetLength() - 1) * 6);

    BoundingBox bounds = heightMap->GetLocalAABB();
    for (UINT z = 0; z < heightMap->GetLength(); ++z)
    {
        for (UINT x = 0; x < heightMap->GetWidth(); ++x)
        {
            float px = static_cast<float>(x) * heightMap->GetCellSpacing();
            float pz = static_cast<float>(z) * heightMap->GetCellSpacing();
            float height = heightMap->SampleHeight(px, pz);
            vertices.push_back({
                XMFLOAT3{ px, height, pz },
                heightMap->SampleNormal(px, pz),
                GetTerrainColor(height, bounds)
                });
        }
    }

    for (UINT z = 0; z + 1 < heightMap->GetLength(); ++z)
    {
        for (UINT x = 0; x + 1 < heightMap->GetWidth(); ++x)
        {
            UINT a = z * heightMap->GetWidth() + x;
            UINT b = a + 1;
            UINT c = (z + 1) * heightMap->GetWidth() + x;
            UINT d = c + 1;

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);

            m_localTriangles.push_back({ vertices[a].position, vertices[c].position, vertices[b].position });
            m_localTriangles.push_back({ vertices[b].position, vertices[c].position, vertices[d].position });
        }
    }

    m_vertices = static_cast<UINT>(vertices.size());
    m_indices = static_cast<UINT>(indices.size());
    m_localAABB = bounds;
    m_localOBB.Center = bounds.Center;
    m_localOBB.Extents = bounds.Extents;
    m_localOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };

    CreateTerrainVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
    CreateTerrainIndexBuffer(device, commandList, indices, m_indexBuffer, m_indexUploadBuffer, m_indexBufferView);
}

TerrainCollider::TerrainCollider(shared_ptr<const TerrainHeightMap> heightMap)
    : Collider(ColliderType::Terrain), m_heightMap(std::move(heightMap))
{
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&m_inverseWorldMatrix, XMMatrixIdentity());
}

void TerrainCollider::Update(const XMFLOAT4X4& worldMatrix)
{
    m_worldMatrix = worldMatrix;
    XMMATRIX world = XMLoadFloat4x4(&m_worldMatrix);
    XMMATRIX inverse = XMMatrixInverse(nullptr, world);
    XMStoreFloat4x4(&m_inverseWorldMatrix, inverse);

    if (!m_heightMap)
    {
        m_worldAABB = BoundingBox{};
        return;
    }

    BoundingBox localAABB = m_heightMap->GetLocalAABB();
    XMFLOAT3 corners[8]{};
    localAABB.GetCorners(corners);
    for (auto& corner : corners) corner = TransformPoint(corner, world);
    BoundingBox::CreateFromPoints(m_worldAABB, 8, corners, sizeof(XMFLOAT3));
}

bool TerrainCollider::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const
{
    if (!m_heightMap) return false;

    XMVECTOR rayOrigin = XMLoadFloat3(&origin);
    XMVECTOR rayDirection = Utiles::Physics::SafeNormalize(XMLoadFloat3(&direction), XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
    float maxDistance = max(m_worldAABB.Extents.x + m_worldAABB.Extents.y + m_worldAABB.Extents.z, 100.0f) * 2.0f;
    float step = max(m_heightMap->GetCellSpacing() * 0.5f, 0.25f);

    for (float distance = 0.0f; distance <= maxDistance; distance += step)
    {
        XMFLOAT3 point{};
        XMStoreFloat3(&point, XMVectorAdd(rayOrigin, XMVectorScale(rayDirection, distance)));
        float height = 0.0f;
        XMFLOAT3 normal{};
        if (GetHeightAtWorld(point, height, normal) && point.y <= height)
        {
            outDist = distance;
            return true;
        }
    }

    return false;
}

bool TerrainCollider::GetHeightAtWorld(const XMFLOAT3& worldPosition, float& outHeight, XMFLOAT3& outNormal) const
{
    if (!m_heightMap) return false;

    XMMATRIX inverse = XMLoadFloat4x4(&m_inverseWorldMatrix);
    XMMATRIX world = XMLoadFloat4x4(&m_worldMatrix);
    XMFLOAT3 local = TransformPoint(worldPosition, inverse);
    if (!m_heightMap->ContainsLocalXZ(local.x, local.z)) return false;

    float localHeight = m_heightMap->SampleHeight(local.x, local.z);
    XMFLOAT3 localPoint{ local.x, localHeight, local.z };
    XMFLOAT3 worldPoint = TransformPoint(localPoint, world);
    outHeight = worldPoint.y;
    outNormal = TransformNormal(m_heightMap->SampleNormal(local.x, local.z), world);
    return true;
}
