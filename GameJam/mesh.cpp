#include "mesh.h"
#include "binaryreader.h"
#include <sstream>

namespace
{
    template <typename Vertex>
    void CreateVertexBuffer(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const vector<Vertex>& vertices,
        ComPtr<ID3D12Resource>& vertexBuffer,
        ComPtr<ID3D12Resource>& vertexUploadBuffer,
        D3D12_VERTEX_BUFFER_VIEW& vertexBufferView)
    {
        const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));

        auto defaultHeapProperties1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto resourceDesc1 = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProperties1,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc1,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer)));

        auto uploadHeapProperties2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc2 = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProperties2,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc2,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexUploadBuffer)));

        D3D12_SUBRESOURCE_DATA vertexData{};
        vertexData.pData = vertices.data();
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;
        UpdateSubresources<1>(commandList.Get(), vertexBuffer.Get(), vertexUploadBuffer.Get(), 0, 0, 1, &vertexData);

        auto resourceBarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        commandList->ResourceBarrier(1, &resourceBarrier1);

        vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexBufferView.SizeInBytes = vertexBufferSize;
        vertexBufferView.StrideInBytes = sizeof(Vertex);
    }

    void CreateIndexBuffer(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const vector<UINT>& indices,
        ComPtr<ID3D12Resource>& indexBuffer,
        ComPtr<ID3D12Resource>& indexUploadBuffer,
        D3D12_INDEX_BUFFER_VIEW& indexBufferView)
    {
        const UINT indexBufferSize = static_cast<UINT>(indices.size() * sizeof(UINT));

        auto defaultHeapProperties3 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto resourceDesc3 = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProperties3,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc3,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&indexBuffer)));

        auto uploadHeapProperties4 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc4 = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
        Utiles::ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProperties4,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc4,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexUploadBuffer)));

        D3D12_SUBRESOURCE_DATA indexData{};
        indexData.pData = indices.data();
        indexData.RowPitch = indexBufferSize;
        indexData.SlicePitch = indexData.RowPitch;
        UpdateSubresources<1>(commandList.Get(), indexBuffer.Get(), indexUploadBuffer.Get(), 0, 0, 1, &indexData);

        auto resourceBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        commandList->ResourceBarrier(1, &resourceBarrier2);

        indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        indexBufferView.SizeInBytes = indexBufferSize;
    }
}

void Mesh::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->DrawInstanced(m_vertices, 1, 0, 0);
}

void Mesh::ReleaseUploadBuffer()
{
    if (m_vertexUploadBuffer) m_vertexUploadBuffer.Reset();
}

void Mesh::CreateBoundingBox(const void* vertices, UINT vertexCount, UINT stride)
{
    if (vertexCount == 0 || !vertices) return;

    XMFLOAT3 vMin{ +FLT_MAX, +FLT_MAX, +FLT_MAX };
    XMFLOAT3 vMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    const BYTE* vertexBytes = static_cast<const BYTE*>(vertices);

    for (UINT i = 0; i < vertexCount; ++i)
    {
        const XMFLOAT3* position = reinterpret_cast<const XMFLOAT3*>(vertexBytes);

        vMin.x = min(vMin.x, position->x);
        vMin.y = min(vMin.y, position->y);
        vMin.z = min(vMin.z, position->z);

        vMax.x = max(vMax.x, position->x);
        vMax.y = max(vMax.y, position->y);
        vMax.z = max(vMax.z, position->z);

        vertexBytes += stride;
    }

    BoundingBox::CreateFromPoints(m_localAABB, XMLoadFloat3(&vMin), XMLoadFloat3(&vMax));

    m_localOBB.Center = m_localAABB.Center;
    m_localOBB.Extents = m_localAABB.Extents;
    m_localOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
}

void IndexMesh::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->DrawIndexedInstanced(m_indices, 1, 0, 0, 0);
}

void IndexMesh::ReleaseUploadBuffer()
{
    Mesh::ReleaseUploadBuffer();
    if (m_indexUploadBuffer) m_indexUploadBuffer.Reset();
}

BinaryMesh::BinaryMesh(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const string& filePath,
    XMFLOAT4 color)
{
    BinaryReader reader{ filesystem::path(filePath) };

    vector<XMFLOAT3> positions;
    vector<XMFLOAT3> normals;
    vector<XMFLOAT4> colors;
    vector<UINT> indices;
    bool hasFileBounds = false;

    while (!reader.End())
    {
        string token = reader.ReadString();

        if (token == "<BoundingBox>:")
        {
            m_localAABB.Center = reader.Read<XMFLOAT3>();
            m_localAABB.Extents = reader.Read<XMFLOAT3>();
            m_localOBB.Center = m_localAABB.Center;
            m_localOBB.Extents = m_localAABB.Extents;
            m_localOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
            hasFileBounds = true;
        }
        else if (token == "<Positions>:")
        {
            int count = reader.Read<int>();
            positions = reader.ReadVector<XMFLOAT3>(max(count, 0));
        }
        else if (token == "<Normals>:")
        {
            int count = reader.Read<int>();
            normals = reader.ReadVector<XMFLOAT3>(max(count, 0));
        }
        else if (token == "<Colors>:")
        {
            int count = reader.Read<int>();
            colors = reader.ReadVector<XMFLOAT4>(max(count, 0));
        }
        else if (token == "<TextureCoords>:")
        {
            int count = reader.Read<int>();
            (void)reader.ReadVector<XMFLOAT2>(max(count, 0));
        }
        else if (token == "<Indices>:")
        {
            int count = reader.Read<int>();
            vector<int32_t> rawIndices = reader.ReadVector<int32_t>(max(count, 0));
            indices.reserve(rawIndices.size());
            for (int32_t index : rawIndices)
            {
                if (index >= 0) indices.push_back(static_cast<UINT>(index));
            }
        }
        else if (token == "<SubMeshes>:")
        {
            int subsetCount = reader.Read<int>();
            for (int i = 0; i < subsetCount; ++i)
            {
                (void)reader.Read<UINT>();
                (void)reader.Read<UINT>();
                int subsetIndexCount = reader.Read<int>();
                (void)reader.ReadVector<int32_t>(max(subsetIndexCount, 0));
            }
            break;
        }
        else
        {
            throw runtime_error("Unknown mesh token: " + token);
        }
    }

    if (positions.empty())
    {
        if (!hasFileBounds)
        {
            throw runtime_error("Binary mesh has no positions: " + filePath);
        }

        XMFLOAT3 center = m_localAABB.Center;
        XMFLOAT3 extents{
            max(m_localAABB.Extents.x, 0.05f),
            max(m_localAABB.Extents.y, 0.05f),
            max(m_localAABB.Extents.z, 0.05f)
        };

        XMFLOAT3 p000{ center.x - extents.x, center.y - extents.y, center.z - extents.z };
        XMFLOAT3 p001{ center.x - extents.x, center.y - extents.y, center.z + extents.z };
        XMFLOAT3 p010{ center.x - extents.x, center.y + extents.y, center.z - extents.z };
        XMFLOAT3 p011{ center.x - extents.x, center.y + extents.y, center.z + extents.z };
        XMFLOAT3 p100{ center.x + extents.x, center.y - extents.y, center.z - extents.z };
        XMFLOAT3 p101{ center.x + extents.x, center.y - extents.y, center.z + extents.z };
        XMFLOAT3 p110{ center.x + extents.x, center.y + extents.y, center.z - extents.z };
        XMFLOAT3 p111{ center.x + extents.x, center.y + extents.y, center.z + extents.z };

        auto addFace = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d, const XMFLOAT3& normal)
        {
            UINT base = static_cast<UINT>(positions.size());
            positions.push_back(a);
            positions.push_back(b);
            positions.push_back(c);
            positions.push_back(d);
            normals.push_back(normal);
            normals.push_back(normal);
            normals.push_back(normal);
            normals.push_back(normal);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        addFace(p100, p101, p111, p110, XMFLOAT3{ 1.0f, 0.0f, 0.0f });
        addFace(p001, p000, p010, p011, XMFLOAT3{ -1.0f, 0.0f, 0.0f });
        addFace(p010, p110, p111, p011, XMFLOAT3{ 0.0f, 1.0f, 0.0f });
        addFace(p000, p001, p101, p100, XMFLOAT3{ 0.0f, -1.0f, 0.0f });
        addFace(p101, p001, p011, p111, XMFLOAT3{ 0.0f, 0.0f, 1.0f });
        addFace(p000, p100, p110, p010, XMFLOAT3{ 0.0f, 0.0f, -1.0f });
    }

    if (normals.size() != positions.size())
    {
        normals.assign(positions.size(), XMFLOAT3{ 0.0f, 1.0f, 0.0f });
    }

    if (colors.size() != positions.size())
    {
        colors.assign(positions.size(), color);
    }

    if (indices.empty())
    {
        indices.reserve(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) indices.push_back(static_cast<UINT>(i));
    }

    vector<Vertex> vertices;
    vertices.reserve(positions.size());
    for (size_t i = 0; i < positions.size(); ++i)
    {
        vertices.push_back({
            positions[i],
            normals[i],
            XMFLOAT4{
                colors[i].x * color.x,
                colors[i].y * color.y,
                colors[i].z * color.z,
                colors[i].w * color.w
            }
            });
    }

    m_localTriangles.clear();
    m_localTriangles.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        UINT a = indices[i];
        UINT b = indices[i + 1];
        UINT c = indices[i + 2];
        if (a < positions.size() && b < positions.size() && c < positions.size())
        {
            m_localTriangles.push_back({ positions[a], positions[b], positions[c] });
        }
    }

    m_vertices = static_cast<UINT>(vertices.size());
    m_indices = static_cast<UINT>(indices.size());

    if (!hasFileBounds) CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
    CreateIndexBuffer(device, commandList, indices, m_indexBuffer, m_indexUploadBuffer, m_indexBufferView);
}

ObjStaticMesh::ObjStaticMesh(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const string& filePath,
    XMFLOAT4 color)
{
    filesystem::path resolvedPath;
    vector<filesystem::path> candidates{
        filesystem::path(filePath),
        filesystem::current_path() / filePath,
        filesystem::current_path() / ".." / filePath,
        filesystem::current_path() / ".." / ".." / filePath
    };

    for (const auto& candidate : candidates)
    {
        if (filesystem::exists(candidate))
        {
            resolvedPath = candidate;
            break;
        }
    }

    if (resolvedPath.empty())
    {
        throw runtime_error("OBJ mesh not found: " + filePath);
    }

    ifstream file(resolvedPath);
    if (!file.is_open())
    {
        throw runtime_error("Failed to open OBJ mesh: " + resolvedPath.string());
    }

    vector<XMFLOAT3> positions;
    vector<Vertex> vertices;
    string line;

    auto parsePositionIndex = [&positions](const string& token) -> int
    {
        size_t slash = token.find('/');
        string indexText = slash == string::npos ? token : token.substr(0, slash);
        if (indexText.empty()) return -1;

        int rawIndex = stoi(indexText);
        if (rawIndex > 0) return rawIndex - 1;
        if (rawIndex < 0) return static_cast<int>(positions.size()) + rawIndex;
        return -1;
    };

    auto addTriangle = [&](int ia, int ib, int ic)
    {
        if (ia < 0 || ib < 0 || ic < 0) return;
        if (ia >= static_cast<int>(positions.size()) ||
            ib >= static_cast<int>(positions.size()) ||
            ic >= static_cast<int>(positions.size())) return;

        const XMFLOAT3& a = positions[ia];
        const XMFLOAT3& b = positions[ib];
        const XMFLOAT3& c = positions[ic];

        XMVECTOR va = XMLoadFloat3(&a);
        XMVECTOR vb = XMLoadFloat3(&b);
        XMVECTOR vc = XMLoadFloat3(&c);
        XMVECTOR normalVector = XMVector3Cross(XMVectorSubtract(vb, va), XMVectorSubtract(vc, va));
        if (Utiles::Physics::VectorLengthSq(normalVector) <= Utiles::Physics::Epsilon)
        {
            normalVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
        else
        {
            normalVector = XMVector3Normalize(normalVector);
        }

        XMFLOAT3 normal{};
        XMStoreFloat3(&normal, normalVector);
        vertices.push_back({ a, normal, color });
        vertices.push_back({ b, normal, color });
        vertices.push_back({ c, normal, color });
        m_localTriangles.push_back({ a, b, c });
    };

    while (getline(file, line))
    {
        if (line.size() < 2) continue;

        if (line[0] == 'v' && line[1] == ' ')
        {
            istringstream stream(line.substr(2));
            XMFLOAT3 position{};
            stream >> position.x >> position.y >> position.z;
            positions.push_back(position);
        }
        else if (line[0] == 'f' && line[1] == ' ')
        {
            istringstream stream(line.substr(2));
            vector<int> polygon;
            string token;
            while (stream >> token)
            {
                polygon.push_back(parsePositionIndex(token));
            }

            if (polygon.size() < 3) continue;
            for (size_t i = 1; i + 1 < polygon.size(); ++i)
            {
                addTriangle(polygon[0], polygon[i], polygon[i + 1]);
            }
        }
    }

    if (vertices.empty())
    {
        throw runtime_error("OBJ mesh has no renderable triangles: " + resolvedPath.string());
    }

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}
CubeMesh::CubeMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    vector<Vertex> vertices;
    vertices.reserve(36);

    const XMFLOAT3 LEFTDOWNFRONT = { -1.f, -1.f, -1.f };
    const XMFLOAT3 LEFTDOWNBACK = { -1.f, -1.f, +1.f };
    const XMFLOAT3 LEFTUPFRONT = { -1.f, +1.f, -1.f };
    const XMFLOAT3 LEFTUPBACK = { -1.f, +1.f, +1.f };
    const XMFLOAT3 RIGHTDOWNFRONT = { +1.f, -1.f, -1.f };
    const XMFLOAT3 RIGHTDOWNBACK = { +1.f, -1.f, +1.f };
    const XMFLOAT3 RIGHTUPFRONT = { +1.f, +1.f, -1.f };
    const XMFLOAT3 RIGHTUPBACK = { +1.f, +1.f, +1.f };

    const XMFLOAT4 cLDF{ 1.0f, 0.0f, 0.0f, 1.0f };
    const XMFLOAT4 cLDB{ 0.0f, 1.0f, 0.0f, 1.0f };
    const XMFLOAT4 cLUF{ 0.0f, 0.0f, 1.0f, 1.0f };
    const XMFLOAT4 cLUB{ 1.0f, 1.0f, 0.0f, 1.0f };
    const XMFLOAT4 cRDF{ 1.0f, 0.0f, 1.0f, 1.0f };
    const XMFLOAT4 cRDB{ 0.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT4 cRUF{ 0.5f, 0.5f, 0.0f, 1.0f };
    const XMFLOAT4 cRUB{ 0.0f, 0.5f, 0.5f, 1.0f };

    auto addFace = [&vertices](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d,
        const XMFLOAT3& normal, const XMFLOAT4& ca, const XMFLOAT4& cb, const XMFLOAT4& cc, const XMFLOAT4& cd)
    {
        vertices.push_back({ a, normal, ca });
        vertices.push_back({ b, normal, cb });
        vertices.push_back({ c, normal, cc });
        vertices.push_back({ a, normal, ca });
        vertices.push_back({ c, normal, cc });
        vertices.push_back({ d, normal, cd });
    };

    addFace(LEFTUPFRONT, RIGHTUPFRONT, RIGHTDOWNFRONT, LEFTDOWNFRONT, { 0.0f, 0.0f, -1.0f }, cLUF, cRUF, cRDF, cLDF);
    addFace(LEFTUPBACK, RIGHTUPBACK, RIGHTUPFRONT, LEFTUPFRONT, { 0.0f, 1.0f, 0.0f }, cLUB, cRUB, cRUF, cLUF);
    addFace(LEFTDOWNBACK, RIGHTDOWNBACK, RIGHTUPBACK, LEFTUPBACK, { 0.0f, 0.0f, 1.0f }, cLDB, cRDB, cRUB, cLUB);
    addFace(LEFTDOWNFRONT, RIGHTDOWNFRONT, RIGHTDOWNBACK, LEFTDOWNBACK, { 0.0f, -1.0f, 0.0f }, cLDF, cRDF, cRDB, cLDB);
    addFace(LEFTUPBACK, LEFTUPFRONT, LEFTDOWNFRONT, LEFTDOWNBACK, { -1.0f, 0.0f, 0.0f }, cLUB, cLUF, cLDF, cLDB);
    addFace(RIGHTUPFRONT, RIGHTUPBACK, RIGHTDOWNBACK, RIGHTDOWNFRONT, { 1.0f, 0.0f, 0.0f }, cRUF, cRUB, cRDB, cRDF);

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}

CubeIndexMesh::CubeIndexMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    vector<Vertex> vertices;
    vector<UINT> indices;
    vertices.reserve(24);
    indices.reserve(36);

    const XMFLOAT3 LEFTDOWNFRONT = { -1.f, -1.f, -1.f };
    const XMFLOAT3 LEFTDOWNBACK = { -1.f, -1.f, +1.f };
    const XMFLOAT3 LEFTUPFRONT = { -1.f, +1.f, -1.f };
    const XMFLOAT3 LEFTUPBACK = { -1.f, +1.f, +1.f };
    const XMFLOAT3 RIGHTDOWNFRONT = { +1.f, -1.f, -1.f };
    const XMFLOAT3 RIGHTDOWNBACK = { +1.f, -1.f, +1.f };
    const XMFLOAT3 RIGHTUPFRONT = { +1.f, +1.f, -1.f };
    const XMFLOAT3 RIGHTUPBACK = { +1.f, +1.f, +1.f };

    const XMFLOAT4 cLDF{ 1.0f, 0.0f, 0.0f, 1.0f };
    const XMFLOAT4 cLDB{ 0.0f, 1.0f, 0.0f, 1.0f };
    const XMFLOAT4 cLUF{ 0.0f, 0.0f, 1.0f, 1.0f };
    const XMFLOAT4 cLUB{ 1.0f, 1.0f, 0.0f, 1.0f };
    const XMFLOAT4 cRDF{ 1.0f, 0.0f, 1.0f, 1.0f };
    const XMFLOAT4 cRDB{ 0.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT4 cRUF{ 0.5f, 0.5f, 0.0f, 1.0f };
    const XMFLOAT4 cRUB{ 0.0f, 0.5f, 0.5f, 1.0f };

    auto addFace = [&vertices, &indices](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d,
        const XMFLOAT3& normal, const XMFLOAT4& ca, const XMFLOAT4& cb, const XMFLOAT4& cc, const XMFLOAT4& cd)
    {
        const UINT baseIndex = static_cast<UINT>(vertices.size());
        vertices.push_back({ a, normal, ca });
        vertices.push_back({ b, normal, cb });
        vertices.push_back({ c, normal, cc });
        vertices.push_back({ d, normal, cd });

        indices.push_back(baseIndex + 0); indices.push_back(baseIndex + 1); indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 0); indices.push_back(baseIndex + 2); indices.push_back(baseIndex + 3);
    };

    addFace(LEFTUPBACK, RIGHTUPBACK, RIGHTUPFRONT, LEFTUPFRONT, { 0.0f, 1.0f, 0.0f }, cLUB, cRUB, cRUF, cLUF);
    addFace(LEFTUPFRONT, RIGHTUPFRONT, RIGHTDOWNFRONT, LEFTDOWNFRONT, { 0.0f, 0.0f, -1.0f }, cLUF, cRUF, cRDF, cLDF);
    addFace(LEFTDOWNFRONT, RIGHTDOWNFRONT, RIGHTDOWNBACK, LEFTDOWNBACK, { 0.0f, -1.0f, 0.0f }, cLDF, cRDF, cRDB, cLDB);
    addFace(RIGHTUPBACK, LEFTUPBACK, LEFTDOWNBACK, RIGHTDOWNBACK, { 0.0f, 0.0f, 1.0f }, cRUB, cLUB, cLDB, cRDB);
    addFace(LEFTUPBACK, LEFTUPFRONT, LEFTDOWNFRONT, LEFTDOWNBACK, { -1.0f, 0.0f, 0.0f }, cLUB, cLUF, cLDF, cLDB);
    addFace(RIGHTUPFRONT, RIGHTUPBACK, RIGHTDOWNBACK, RIGHTDOWNFRONT, { 1.0f, 0.0f, 0.0f }, cRUF, cRUB, cRDB, cRDF);

    m_vertices = static_cast<UINT>(vertices.size());
    m_indices = static_cast<UINT>(indices.size());

    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
    CreateIndexBuffer(device, commandList, indices, m_indexBuffer, m_indexUploadBuffer, m_indexBufferView);
}

PlaneMesh::PlaneMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    vector<Vertex> vertices;

    float w = 15.0f;
    float d = 15.0f;
    float y = 0.0f;

    XMFLOAT3 leftTop = { -w, y, +d };
    XMFLOAT3 rightTop = { +w, y, +d };
    XMFLOAT3 leftBottom = { -w, y, -d };
    XMFLOAT3 rightBottom = { +w, y, -d };

    XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    XMFLOAT4 planeColor = { 0.4f, 0.6f, 0.4f, 1.0f };

    vertices.push_back({ leftTop, normal, planeColor });
    vertices.push_back({ rightTop, normal, planeColor });
    vertices.push_back({ rightBottom, normal, planeColor });

    vertices.push_back({ leftTop, normal, planeColor });
    vertices.push_back({ rightBottom, normal, planeColor });
    vertices.push_back({ leftBottom, normal, planeColor });

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}

CapsuleIndexMesh::CapsuleIndexMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList, float radius, float height, int segments)
{
    vector<Vertex> vertices;
    vector<UINT> indices;

    int rings = segments / 2;
    float halfHeight = height * 0.5f;
    XMFLOAT4 color = { 0.8f, 0.2f, 0.2f, 1.0f };

    for (int i = 0; i <= rings; ++i)
    {
        float theta = XM_PIDIV2 - (static_cast<float>(i) / rings) * XM_PIDIV2;
        for (int j = 0; j <= segments; ++j)
        {
            float phi = (static_cast<float>(j) / segments) * XM_2PI;
            XMFLOAT3 normal{ cosf(theta) * cosf(phi), sinf(theta), cosf(theta) * sinf(phi) };
            XMFLOAT3 pos{ radius * normal.x, radius * normal.y + halfHeight, radius * normal.z };
            vertices.push_back({ pos, normal, color });
        }
    }

    for (int i = 0; i <= rings; ++i)
    {
        float theta = -(static_cast<float>(i) / rings) * XM_PIDIV2;
        for (int j = 0; j <= segments; ++j)
        {
            float phi = (static_cast<float>(j) / segments) * XM_2PI;
            XMFLOAT3 normal{ cosf(theta) * cosf(phi), sinf(theta), cosf(theta) * sinf(phi) };
            XMFLOAT3 pos{ radius * normal.x, radius * normal.y - halfHeight, radius * normal.z };
            vertices.push_back({ pos, normal, color });
        }
    }

    int ringVertexCount = segments + 1;
    for (int i = 0; i < (rings * 2 + 1); ++i)
    {
        for (int j = 0; j < segments; ++j)
        {
            indices.push_back(i * ringVertexCount + j);
            indices.push_back(i * ringVertexCount + j + 1);
            indices.push_back((i + 1) * ringVertexCount + j);

            indices.push_back(i * ringVertexCount + j + 1);
            indices.push_back((i + 1) * ringVertexCount + j + 1);
            indices.push_back((i + 1) * ringVertexCount + j);
        }
    }

    m_vertices = static_cast<UINT>(vertices.size());
    m_indices = static_cast<UINT>(indices.size());

    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
    CreateIndexBuffer(device, commandList, indices, m_indexBuffer, m_indexUploadBuffer, m_indexBufferView);
}

FirstPersonGunMesh::FirstPersonGunMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    vector<Vertex> vertices;
    vertices.reserve(36 * 4);

    auto addBox = [&vertices](XMFLOAT3 minCorner, XMFLOAT3 maxCorner, XMFLOAT4 color)
    {
        XMFLOAT3 ldf{ minCorner.x, minCorner.y, minCorner.z };
        XMFLOAT3 ldb{ minCorner.x, minCorner.y, maxCorner.z };
        XMFLOAT3 luf{ minCorner.x, maxCorner.y, minCorner.z };
        XMFLOAT3 lub{ minCorner.x, maxCorner.y, maxCorner.z };
        XMFLOAT3 rdf{ maxCorner.x, minCorner.y, minCorner.z };
        XMFLOAT3 rdb{ maxCorner.x, minCorner.y, maxCorner.z };
        XMFLOAT3 ruf{ maxCorner.x, maxCorner.y, minCorner.z };
        XMFLOAT3 rub{ maxCorner.x, maxCorner.y, maxCorner.z };

        auto addFace = [&vertices, color](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d, XMFLOAT3 normal)
        {
            vertices.push_back({ a, normal, color });
            vertices.push_back({ b, normal, color });
            vertices.push_back({ c, normal, color });
            vertices.push_back({ a, normal, color });
            vertices.push_back({ c, normal, color });
            vertices.push_back({ d, normal, color });
        };

        addFace(luf, ruf, rdf, ldf, { 0.0f, 0.0f, -1.0f });
        addFace(lub, rub, ruf, luf, { 0.0f, 1.0f, 0.0f });
        addFace(ldb, rdb, rub, lub, { 0.0f, 0.0f, 1.0f });
        addFace(ldf, rdf, rdb, ldb, { 0.0f, -1.0f, 0.0f });
        addFace(lub, luf, ldf, ldb, { -1.0f, 0.0f, 0.0f });
        addFace(ruf, rub, rdb, rdf, { 1.0f, 0.0f, 0.0f });
    };

    addBox({ 0.26f, -0.34f, 0.55f }, { 0.54f, -0.22f, 1.10f }, { 0.16f, 0.17f, 0.18f, 1.0f });
    addBox({ 0.34f, -0.26f, 1.00f }, { 0.46f, -0.18f, 1.35f }, { 0.07f, 0.08f, 0.09f, 1.0f });
    addBox({ 0.34f, -0.58f, 0.58f }, { 0.46f, -0.32f, 0.78f }, { 0.09f, 0.08f, 0.07f, 1.0f });
    addBox({ 0.36f, -0.19f, 0.72f }, { 0.44f, -0.14f, 0.92f }, { 0.05f, 0.18f, 0.15f, 1.0f });

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}

CrosshairMesh::CrosshairMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    vector<Vertex> vertices;
    vertices.reserve(12);

    constexpr float distance = 0.60f;
    constexpr float length = 0.026f;
    constexpr float thickness = 0.0025f;
    const XMFLOAT3 normal{ 0.0f, 0.0f, -1.0f };
    const XMFLOAT4 color{ 0.92f, 0.98f, 1.0f, 1.0f };

    auto addQuad = [&vertices, normal, color, distance](float left, float top, float right, float bottom)
    {
        XMFLOAT3 lt{ left, top, distance };
        XMFLOAT3 rt{ right, top, distance };
        XMFLOAT3 rb{ right, bottom, distance };
        XMFLOAT3 lb{ left, bottom, distance };

        vertices.push_back({ lt, normal, color });
        vertices.push_back({ rt, normal, color });
        vertices.push_back({ rb, normal, color });
        vertices.push_back({ lt, normal, color });
        vertices.push_back({ rb, normal, color });
        vertices.push_back({ lb, normal, color });
    };

    addQuad(-length, thickness, length, -thickness);
    addQuad(-thickness, length, thickness, -length);

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}

BulletMesh::BulletMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    vector<Vertex> vertices;
    vertices.reserve(36);

    const XMFLOAT4 color{ 1.0f, 0.82f, 0.22f, 1.0f };
    const XMFLOAT3 minCorner{ -0.035f, -0.035f, -0.10f };
    const XMFLOAT3 maxCorner{ 0.035f, 0.035f, 0.24f };

    XMFLOAT3 ldf{ minCorner.x, minCorner.y, minCorner.z };
    XMFLOAT3 ldb{ minCorner.x, minCorner.y, maxCorner.z };
    XMFLOAT3 luf{ minCorner.x, maxCorner.y, minCorner.z };
    XMFLOAT3 lub{ minCorner.x, maxCorner.y, maxCorner.z };
    XMFLOAT3 rdf{ maxCorner.x, minCorner.y, minCorner.z };
    XMFLOAT3 rdb{ maxCorner.x, minCorner.y, maxCorner.z };
    XMFLOAT3 ruf{ maxCorner.x, maxCorner.y, minCorner.z };
    XMFLOAT3 rub{ maxCorner.x, maxCorner.y, maxCorner.z };

    auto addFace = [&vertices, color](const XMFLOAT3& a, const XMFLOAT3& b,
        const XMFLOAT3& c, const XMFLOAT3& d, XMFLOAT3 normal)
    {
        vertices.push_back({ a, normal, color });
        vertices.push_back({ b, normal, color });
        vertices.push_back({ c, normal, color });
        vertices.push_back({ a, normal, color });
        vertices.push_back({ c, normal, color });
        vertices.push_back({ d, normal, color });
    };

    addFace(luf, ruf, rdf, ldf, { 0.0f, 0.0f, -1.0f });
    addFace(lub, rub, ruf, luf, { 0.0f, 1.0f, 0.0f });
    addFace(ldb, rdb, rub, lub, { 0.0f, 0.0f, 1.0f });
    addFace(ldf, rdf, rdb, ldb, { 0.0f, -1.0f, 0.0f });
    addFace(lub, luf, ldf, ldb, { -1.0f, 0.0f, 0.0f });
    addFace(ruf, rub, rdb, rdf, { 1.0f, 0.0f, 0.0f });

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}

StairMesh::StairMesh(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList,
    float width, float stepHeight, float stepDepth, int steps)
{
    vector<Vertex> vertices;

    steps = max(1, steps);
    width = max(0.1f, width);
    stepHeight = max(0.05f, stepHeight);
    stepDepth = max(0.05f, stepDepth);

    vertices.reserve(static_cast<size_t>(steps) * 48);
    m_localTriangles.clear();
    m_localTriangles.reserve(static_cast<size_t>(steps) * 16);

    const float halfWidth = width * 0.5f;
    const float totalDepth = stepDepth * steps;
    const float totalHeight = stepHeight * steps;
    const XMFLOAT4 color{ 0.52f, 0.51f, 0.48f, 1.0f };

    auto addTriangle = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& normal)
    {
        vertices.push_back({ a, normal, color });
        vertices.push_back({ b, normal, color });
        vertices.push_back({ c, normal, color });
        m_localTriangles.push_back({ a, b, c });
    };

    auto addQuad = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d, const XMFLOAT3& normal)
    {
        addTriangle(a, c, b, normal);
        addTriangle(a, d, c, normal);
    };

    for (int i = 0; i < steps; ++i)
    {
        const float z0 = i * stepDepth;
        const float z1 = (i + 1) * stepDepth;
        const float y0 = i * stepHeight;
        const float y1 = (i + 1) * stepHeight;

        addQuad({ -halfWidth, y1, z0 }, { halfWidth, y1, z0 }, { halfWidth, y1, z1 }, { -halfWidth, y1, z1 }, { 0.0f, 1.0f, 0.0f });
        addQuad({ -halfWidth, y1, z0 }, { -halfWidth, y0, z0 }, { halfWidth, y0, z0 }, { halfWidth, y1, z0 }, { 0.0f, 0.0f, -1.0f });

        addQuad({ -halfWidth, 0.0f, z0 }, { -halfWidth, y1, z0 }, { -halfWidth, y1, z1 }, { -halfWidth, 0.0f, z1 }, { -1.0f, 0.0f, 0.0f });
        addQuad({ halfWidth, y1, z0 }, { halfWidth, 0.0f, z0 }, { halfWidth, 0.0f, z1 }, { halfWidth, y1, z1 }, { 1.0f, 0.0f, 0.0f });
    }

    addQuad({ -halfWidth, 0.0f, totalDepth }, { -halfWidth, totalHeight, totalDepth },
        { halfWidth, totalHeight, totalDepth }, { halfWidth, 0.0f, totalDepth }, { 0.0f, 0.0f, 1.0f });
    addQuad({ -halfWidth, 0.0f, 0.0f }, { -halfWidth, 0.0f, totalDepth },
        { halfWidth, 0.0f, totalDepth }, { halfWidth, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f });

    m_vertices = static_cast<UINT>(vertices.size());
    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
}

namespace
{
    filesystem::path GetModuleDirectoryForMeshLoading()
    {
        WCHAR modulePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
        if (length == 0 || length == _countof(modulePath)) return {};

        return filesystem::path(modulePath).parent_path();
    }

    string ToLowerCopy(string value)
    {
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
        return value;
    }

    bool StartsWith(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
    }

    filesystem::path ResolveMeshPath(const filesystem::path& requested)
    {
        if (requested.is_absolute() && filesystem::exists(requested)) return requested;
        if (filesystem::exists(requested)) return filesystem::absolute(requested);

        vector<filesystem::path> roots;
        roots.push_back(filesystem::current_path());

        filesystem::path current = filesystem::current_path();
        for (int i = 0; i < 4 && current.has_parent_path(); ++i)
        {
            current = current.parent_path();
            roots.push_back(current);
        }

        filesystem::path moduleDirectory = GetModuleDirectoryForMeshLoading();
        if (!moduleDirectory.empty())
        {
            roots.push_back(moduleDirectory);
            current = moduleDirectory;
            for (int i = 0; i < 5 && current.has_parent_path(); ++i)
            {
                current = current.parent_path();
                roots.push_back(current);
            }
        }

        for (const auto& root : roots)
        {
            filesystem::path candidate = root / requested;
            if (filesystem::exists(candidate)) return filesystem::absolute(candidate);
        }

        return {};
    }

    filesystem::path ResolveConvertedFbxTextPath(const filesystem::path& modelPath)
    {
        string extension = ToLowerCopy(modelPath.extension().string());
        if (extension == ".txt" && filesystem::exists(modelPath)) return modelPath;

        vector<filesystem::path> candidates;
        if (extension == ".fbx")
        {
            candidates.push_back(modelPath.parent_path().parent_path() / (modelPath.stem().string() + ".txt"));
            candidates.push_back(modelPath.parent_path() / (modelPath.stem().string() + ".txt"));
        }

        filesystem::path replaced = modelPath;
        replaced.replace_extension(".txt");
        candidates.push_back(replaced);

        for (const auto& candidate : candidates)
        {
            if (filesystem::exists(candidate)) return filesystem::absolute(candidate);
        }

        for (const auto& candidate : candidates)
        {
            filesystem::path resolved = ResolveMeshPath(candidate);
            if (!resolved.empty()) return resolved;
        }

        return {};
    }

    XMFLOAT4 GetConvertedFbxFrameColor(const string& frameName)
    {
        if (frameName == "rotor") return XMFLOAT4{ 0.86f, 0.13f, 0.18f, 1.0f };
        if (frameName == "black_m_7") return XMFLOAT4{ 0.16f, 0.68f, 0.36f, 1.0f };
        if (frameName == "black_m_6") return XMFLOAT4{ 0.92f, 0.56f, 0.08f, 1.0f };
        if (frameName == "glass") return XMFLOAT4{ 0.34f, 0.74f, 0.92f, 1.0f };
        if (StartsWith(frameName, "rocket")) return XMFLOAT4{ 0.30f, 0.32f, 0.34f, 1.0f };
        if (frameName == "gold") return XMFLOAT4{ 0.90f, 0.66f, 0.18f, 1.0f };
        if (frameName == "silver") return XMFLOAT4{ 0.68f, 0.70f, 0.72f, 1.0f };
        return XMFLOAT4{ 0.22f, 0.30f, 0.39f, 1.0f };
    }

    void SkipFloatValues(ifstream& stream, int count)
    {
        float ignored = 0.0f;
        for (int i = 0; i < count; ++i) stream >> ignored;
    }

    XMFLOAT3 TransformConvertedPosition(const XMFLOAT3& position, CXMMATRIX matrix)
    {
        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVector3TransformCoord(XMLoadFloat3(&position), matrix));
        return result;
    }

    XMFLOAT3 TransformConvertedNormal(const XMFLOAT3& normal, CXMMATRIX matrix)
    {
        XMVECTOR transformed = XMVector3TransformNormal(XMLoadFloat3(&normal), matrix);
        if (XMVectorGetX(XMVector3LengthSq(transformed)) <= 1.0e-8f)
        {
            return XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        }

        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVector3Normalize(transformed));
        return result;
    }

    XMMATRIX ReadConvertedTransformMatrix(ifstream& stream)
    {
        XMFLOAT4X4 transform{};
        stream >> transform._11 >> transform._12 >> transform._13 >> transform._14;
        stream >> transform._21 >> transform._22 >> transform._23 >> transform._24;
        stream >> transform._31 >> transform._32 >> transform._33 >> transform._34;
        stream >> transform._41 >> transform._42 >> transform._43 >> transform._44;
        return XMLoadFloat4x4(&transform);
    }

    float GetPointAxisValue(const XMFLOAT3& point, int axis)
    {
        if (axis == 0) return point.x;
        if (axis == 1) return point.y;
        return point.z;
    }

    float GetBoxExtentByAxis(const XMFLOAT3& extents, int axis)
    {
        if (axis == 0) return extents.x;
        if (axis == 1) return extents.y;
        return extents.z;
    }

    int GetLongestAxis(const BoundingBox& box)
    {
        int axis = 0;
        float extent = box.Extents.x;
        if (box.Extents.y > extent)
        {
            axis = 1;
            extent = box.Extents.y;
        }
        if (box.Extents.z > extent) axis = 2;
        return axis;
    }

    bool IsLongApacheBodyShellPart(const string& frameName, const BoundingBox& localAABB)
    {
        if (frameName != "black_m" &&
            frameName != "body" &&
            frameName != "gum" &&
            frameName != "side" &&
            frameName != "silver" &&
            frameName != "skin")
        {
            return false;
        }

        int axis = GetLongestAxis(localAABB);
        return GetBoxExtentByAxis(localAABB.Extents, axis) * 2.0f > 36.0f;
    }

    bool IsProblemApacheColliderName(const string& name)
    {
        if (name == "silver") return true;
        if (StartsWith(name, "silver:")) return true;
        if (name.find(":silver") != string::npos) return true;
        return false;
    }

    bool ShouldBuildApacheColliderPart(const string& frameName, const string& meshName, const string& partName)
    {
        return !IsProblemApacheColliderName(frameName) &&
            !IsProblemApacheColliderName(meshName) &&
            !IsProblemApacheColliderName(partName);
    }

    bool IsApacheMainRotorBladePart(const string& frameName, const string& meshName, const string& partName)
    {
        return frameName == "rotor" || meshName == "rotor" || StartsWith(partName, "rotor");
    }

    bool IsApacheMainRotorHubPart(const string& frameName, const string& partName)
    {
        return frameName == "black_m_6" || partName == "black_m_6" || StartsWith(partName, "black_m_6:");
    }

    bool IsApacheMainRotorVisualPart(const string& frameName, const string& meshName, const string& partName)
    {
        return IsApacheMainRotorBladePart(frameName, meshName, partName) ||
            IsApacheMainRotorHubPart(frameName, partName);
    }

    bool IsApacheTailRotorVisualPart(const string& frameName, const string& meshName, const string& partName)
    {
        return frameName == "black_m_7" ||
            meshName == "black_m_7" ||
            partName == "black_m_7" ||
            StartsWith(partName, "black_m_7:");
    }

    bool ShouldRenderConvertedPart(FbxStaticMeshFilter filter,
        const string& frameName,
        const string& meshName,
        const string& partName)
    {
        bool isMainRotor = IsApacheMainRotorVisualPart(frameName, meshName, partName);
        bool isTailRotor = IsApacheTailRotorVisualPart(frameName, meshName, partName);
        if (filter == FbxStaticMeshFilter::ExcludeApacheAnimatedRotors) return !isMainRotor && !isTailRotor;
        if (filter == FbxStaticMeshFilter::ExcludeApacheMainRotor) return !isMainRotor;
        if (filter == FbxStaticMeshFilter::ApacheMainRotorOnly) return isMainRotor;
        if (filter == FbxStaticMeshFilter::ApacheTailRotorOnly) return isTailRotor;
        return true;
    }

    bool ShouldSplitApacheMainRotorHub(FbxStaticMeshFilter filter,
        const string& frameName,
        const string& partName)
    {
        return false;
    }

    bool IsApacheMainRotorHubTriangle(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
    {
        constexpr XMFLOAT3 pivot{ 2.172522f, 13.23573f, -8.805087f };
        constexpr FLOAT rotatingHubRadius = 7.8f;
        constexpr FLOAT rotatingHubMinY = pivot.y - 0.55f;

        XMFLOAT3 center{
            (a.x + b.x + c.x) / 3.0f,
            (a.y + b.y + c.y) / 3.0f,
            (a.z + b.z + c.z) / 3.0f
        };

        FLOAT dx = center.x - pivot.x;
        FLOAT dz = center.z - pivot.z;
        FLOAT radiusSq = dx * dx + dz * dz;
        return center.y >= rotatingHubMinY &&
            radiusSq <= rotatingHubRadius * rotatingHubRadius;
    }

    vector<UINT> BuildFilteredConvertedIndices(FbxStaticMeshFilter filter,
        bool splitMainRotorHub,
        const vector<UINT>& meshIndices,
        const vector<XMFLOAT3>& transformedPositions)
    {
        if (!splitMainRotorHub) return meshIndices;

        vector<UINT> filteredIndices;
        filteredIndices.reserve(meshIndices.size());

        size_t triangleIndexCount = meshIndices.size() - (meshIndices.size() % 3);
        for (size_t i = 0; i < triangleIndexCount; i += 3)
        {
            UINT i0 = meshIndices[i + 0];
            UINT i1 = meshIndices[i + 1];
            UINT i2 = meshIndices[i + 2];
            if (i0 >= transformedPositions.size() ||
                i1 >= transformedPositions.size() ||
                i2 >= transformedPositions.size())
            {
                continue;
            }

            bool isRotatingHubTriangle = IsApacheMainRotorHubTriangle(
                transformedPositions[i0],
                transformedPositions[i1],
                transformedPositions[i2]);

            if (filter == FbxStaticMeshFilter::ApacheMainRotorOnly && !isRotatingHubTriangle) continue;
            if (filter == FbxStaticMeshFilter::ExcludeApacheMainRotor && isRotatingHubTriangle) continue;

            filteredIndices.push_back(i0);
            filteredIndices.push_back(i1);
            filteredIndices.push_back(i2);
        }

        return filteredIndices;
    }

    void PushAxisAlignedPartBounds(const string& partName,
        const vector<XMFLOAT3>& points,
        vector<MeshPartBounds>& partBounds)
    {
        if (points.empty()) return;

        BoundingBox localAABB{};
        BoundingBox::CreateFromPoints(localAABB, points.size(), points.data(), sizeof(XMFLOAT3));

        BoundingOrientedBox localOBB{};
        localOBB.Center = localAABB.Center;
        localOBB.Extents = localAABB.Extents;
        localOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
         
        partBounds.push_back({ partName, localAABB, localOBB });
    }

    void PushSplitMainRotorPartBounds(const string& partName,
        const vector<XMFLOAT3>& transformedPositions,
        vector<MeshPartBounds>& partBounds)
    {
        if (transformedPositions.empty()) return;

        BoundingBox rotorAABB{};
        BoundingBox::CreateFromPoints(rotorAABB,
            transformedPositions.size(),
            transformedPositions.data(),
            sizeof(XMFLOAT3));

        vector<XMFLOAT3> bladePoints[2];
        bladePoints[0].reserve(transformedPositions.size() / 2);
        bladePoints[1].reserve(transformedPositions.size() / 2);

        for (const auto& point : transformedPositions)
        {
            float distanceX = fabsf(point.x - rotorAABB.Center.x);
            float distanceZ = fabsf(point.z - rotorAABB.Center.z);
            bladePoints[distanceX >= distanceZ ? 0 : 1].push_back(point);
        }

        PushAxisAlignedPartBounds(partName + "#bladeX", bladePoints[0], partBounds);
        PushAxisAlignedPartBounds(partName + "#bladeZ", bladePoints[1], partBounds);
    }

    void PushSplitBodyShellPartBounds(const string& partName,
        const BoundingBox& localAABB,
        const vector<XMFLOAT3>& transformedPositions,
        vector<MeshPartBounds>& partBounds)
    {
        int axis = GetLongestAxis(localAABB);
        float minValue = GetPointAxisValue(localAABB.Center, axis) - GetBoxExtentByAxis(localAABB.Extents, axis);
        float fullLength = GetBoxExtentByAxis(localAABB.Extents, axis) * 2.0f;
        if (fullLength <= 0.0f)
        {
            PushAxisAlignedPartBounds(partName, transformedPositions, partBounds);
            return;
        }

        int sliceCount = static_cast<int>(ceilf(fullLength / 28.0f));
        sliceCount = clamp(sliceCount, 2, 6);
        float sliceLength = fullLength / static_cast<float>(sliceCount);

        for (int i = 0; i < sliceCount; ++i)
        {
            float sliceMin = minValue + sliceLength * static_cast<float>(i);
            float sliceMax = i == sliceCount - 1 ? minValue + fullLength : sliceMin + sliceLength;

            vector<XMFLOAT3> slicePoints;
            slicePoints.reserve(transformedPositions.size() / static_cast<size_t>(sliceCount) + 8);
            for (const auto& point : transformedPositions)
            {
                float value = GetPointAxisValue(point, axis);
                if (value < sliceMin || value > sliceMax) continue;
                slicePoints.push_back(point);
            }

            if (slicePoints.empty()) continue;

            PushAxisAlignedPartBounds(partName + "#" + to_string(i + 1), slicePoints, partBounds);
        }
    }

    void AppendConvertedMesh(ifstream& stream,
        const string& frameName,
        CXMMATRIX frameWorld,
        FbxStaticMeshFilter filter,
        vector<FbxStaticMesh::Vertex>& vertices,
        vector<UINT>& indices,
        vector<MeshPartBounds>& partBounds)
    {
        int declaredVertexCount = 0;
        string meshName;
        stream >> declaredVertexCount >> meshName;

        vector<XMFLOAT3> positions;
        vector<XMFLOAT3> normals;
        vector<UINT> meshIndices;
        BoundingOrientedBox sourceOBB{};
        bool hasSourceBounds = false;

        string token;
        while (stream >> token)
        {
            if (token == "<Bounds>:")
            {
                stream >> sourceOBB.Center.x >> sourceOBB.Center.y >> sourceOBB.Center.z;
                stream >> sourceOBB.Extents.x >> sourceOBB.Extents.y >> sourceOBB.Extents.z;
                sourceOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
                hasSourceBounds = true;
            }
            else if (token == "<Positions>:")
            {
                int count = 0;
                stream >> count;
                positions.resize(max(count, 0));
                for (auto& position : positions)
                {
                    stream >> position.x >> position.y >> position.z;
                }
            }
            else if (token == "<Colors>:")
            {
                int count = 0;
                stream >> count;
                SkipFloatValues(stream, max(count, 0) * 4);
            }
            else if (token == "<Normals>:")
            {
                int count = 0;
                stream >> count;
                normals.resize(max(count, 0));
                for (auto& normal : normals)
                {
                    stream >> normal.x >> normal.y >> normal.z;
                }
            }
            else if (token == "<Indices>:")
            {
                int count = 0;
                stream >> count;
                meshIndices.reserve(meshIndices.size() + max(count, 0));
                for (int i = 0; i < count; ++i)
                {
                    UINT index = 0;
                    stream >> index;
                    meshIndices.push_back(index);
                }
            }
            else if (token == "<SubMeshes>:")
            {
                int subMeshCount = 0;
                stream >> subMeshCount;
                for (int i = 0; i < subMeshCount; ++i)
                {
                    string subMeshToken;
                    stream >> subMeshToken;
                    if (subMeshToken != "<SubMesh>:") continue;

                    string subsetName;
                    int indexCount = 0;
                    stream >> subsetName >> indexCount;
                    meshIndices.reserve(meshIndices.size() + max(indexCount, 0));
                    for (int j = 0; j < indexCount; ++j)
                    {
                        UINT index = 0;
                        stream >> index;
                        meshIndices.push_back(index);
                    }
                }
            }
            else if (token == "</Mesh>")
            {
                break;
            }
        }

        if (positions.empty()) return;
        if (normals.size() != positions.size())
        {
            normals.assign(positions.size(), XMFLOAT3{ 0.0f, 1.0f, 0.0f });
        }

        string partName = frameName.empty() ? meshName : frameName;
        if (!meshName.empty() && meshName != frameName) partName += ":" + meshName;
        if (!ShouldRenderConvertedPart(filter, frameName, meshName, partName)) return;

        vector<XMFLOAT3> transformedPositions;
        vector<XMFLOAT3> transformedNormals;
        transformedPositions.reserve(positions.size());
        transformedNormals.reserve(positions.size());
        for (size_t i = 0; i < positions.size(); ++i)
        {
            transformedPositions.push_back(TransformConvertedPosition(positions[i], frameWorld));
            transformedNormals.push_back(TransformConvertedNormal(normals[i], frameWorld));
        }

        if (meshIndices.empty())
        {
            meshIndices.reserve(positions.size());
            for (size_t i = 0; i < positions.size(); ++i) meshIndices.push_back(static_cast<UINT>(i));
        }

        bool splitMainRotorHub = ShouldSplitApacheMainRotorHub(filter, frameName, partName);
        vector<UINT> filteredMeshIndices = BuildFilteredConvertedIndices(
            filter,
            splitMainRotorHub,
            meshIndices,
            transformedPositions);
        if (filteredMeshIndices.empty()) return;

        UINT baseIndex = static_cast<UINT>(vertices.size());
        XMFLOAT4 color = GetConvertedFbxFrameColor(frameName);
        vector<UINT> remappedIndices(positions.size(), UINT_MAX);
        vector<XMFLOAT3> usedTransformedPositions;
        usedTransformedPositions.reserve(positions.size());
        vertices.reserve(vertices.size() + positions.size());

        indices.reserve(indices.size() + filteredMeshIndices.size());
        for (UINT sourceIndex : filteredMeshIndices)
        {
            if (sourceIndex >= positions.size()) continue;

            if (remappedIndices[sourceIndex] == UINT_MAX)
            {
                remappedIndices[sourceIndex] = baseIndex + static_cast<UINT>(usedTransformedPositions.size());
                usedTransformedPositions.push_back(transformedPositions[sourceIndex]);

                vertices.push_back({
                    transformedPositions[sourceIndex],
                    transformedNormals[sourceIndex],
                    color
                    });
            }

            indices.push_back(remappedIndices[sourceIndex]);
        }

        if (!usedTransformedPositions.empty())
        {
            BoundingBox localAABB{};
            BoundingBox::CreateFromPoints(localAABB,
                usedTransformedPositions.size(),
                usedTransformedPositions.data(),
                sizeof(XMFLOAT3));

            BoundingOrientedBox localOBB{};
            if (hasSourceBounds && !splitMainRotorHub)
            {
                sourceOBB.Transform(localOBB, frameWorld);
            }
            else
            {
                localOBB.Center = localAABB.Center;
                localOBB.Extents = localAABB.Extents;
                localOBB.Orientation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
            }

            if (!ShouldBuildApacheColliderPart(frameName, meshName, partName))
            {
                // Broad material groups are visually useful but too noisy for physics.
            }
            else if (IsApacheMainRotorBladePart(frameName, meshName, partName))
            {
                PushSplitMainRotorPartBounds(partName, usedTransformedPositions, partBounds);
            }
            else if (IsLongApacheBodyShellPart(frameName, localAABB))
            {
                PushSplitBodyShellPartBounds(partName, localAABB, usedTransformedPositions, partBounds);
            }
            else
            {
                partBounds.push_back({ partName, localAABB, localOBB });
            }
        }
    }

    void SkipConvertedBlock(ifstream& stream, const string& closingToken)
    {
        string token;
        while (stream >> token)
        {
            if (token == closingToken) break;
        }
    }

    void ReadConvertedFrame(ifstream& stream,
        CXMMATRIX parentWorld,
        FbxStaticMeshFilter filter,
        vector<FbxStaticMesh::Vertex>& vertices,
        vector<UINT>& indices,
        vector<MeshPartBounds>& partBounds)
    {
        int frameIndex = 0;
        string frameName;
        stream >> frameIndex >> frameName;

        XMMATRIX frameWorld = parentWorld;
        string token;
        while (stream >> token)
        {
            if (token == "<Transform>:")
            {
                SkipFloatValues(stream, 13);
            }
            else if (token == "<TransformMatrix>:")
            {
                frameWorld = ReadConvertedTransformMatrix(stream) * parentWorld;
            }
            else if (token == "<Mesh>:")
            {
                AppendConvertedMesh(stream, frameName, frameWorld, filter, vertices, indices, partBounds);
            }
            else if (token == "<Materials>:")
            {
                SkipConvertedBlock(stream, "</Materials>");
            }
            else if (token == "<Children>:")
            {
                int childCount = 0;
                stream >> childCount;
                for (int i = 0; i < childCount; ++i)
                {
                    string childToken;
                    stream >> childToken;
                    if (childToken == "<Frame>:")
                    {
                        ReadConvertedFrame(stream, frameWorld, filter, vertices, indices, partBounds);
                    }
                }
            }
            else if (token == "</Frame>")
            {
                break;
            }
        }
    }

    void LoadConvertedFbxTextMesh(const filesystem::path& path,
        FbxStaticMeshFilter filter,
        vector<FbxStaticMesh::Vertex>& vertices,
        vector<UINT>& indices,
        vector<MeshPartBounds>& partBounds)
    {
        ifstream stream(path);
        if (!stream) throw runtime_error("Failed to open converted FBX text mesh: " + path.string());

        string token;
        while (stream >> token)
        {
            if (token == "<Frame>:")
            {
                ReadConvertedFrame(stream, XMMatrixIdentity(), filter, vertices, indices, partBounds);
            }
        }
    }
}

FbxStaticMesh::FbxStaticMesh(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const string& filePath,
    FbxStaticMeshFilter filter)
{
    filesystem::path sourcePath = ResolveMeshPath(filePath);
    if (sourcePath.empty())
    {
        throw runtime_error("FBX model file not found: " + filePath);
    }

    filesystem::path convertedTextPath = ResolveConvertedFbxTextPath(sourcePath);
    if (convertedTextPath.empty())
    {
        throw runtime_error("Compressed binary FBX requires a converted text mesh next to the FBX: " + sourcePath.string());
    }

    vector<Vertex> vertices;
    vector<UINT> indices;
    m_localParts.clear();
    LoadConvertedFbxTextMesh(convertedTextPath, filter, vertices, indices, m_localParts);

    if (vertices.empty() || indices.empty())
    {
        throw runtime_error("Converted FBX mesh has no drawable geometry: " + convertedTextPath.string());
    }

    m_localTriangles.clear();
    m_localTriangles.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        UINT a = indices[i];
        UINT b = indices[i + 1];
        UINT c = indices[i + 2];
        if (a < vertices.size() && b < vertices.size() && c < vertices.size())
        {
            m_localTriangles.push_back({ vertices[a].position, vertices[b].position, vertices[c].position });
        }
    }

    m_vertices = static_cast<UINT>(vertices.size());
    m_indices = static_cast<UINT>(indices.size());

    CreateBoundingBox(vertices.data(), m_vertices, sizeof(Vertex));
    CreateVertexBuffer(device, commandList, vertices, m_vertexBuffer, m_vertexUploadBuffer, m_vertexBufferView);
    CreateIndexBuffer(device, commandList, indices, m_indexBuffer, m_indexUploadBuffer, m_indexBufferView);
}
