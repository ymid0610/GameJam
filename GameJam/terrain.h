#pragma once

#include "stdafx.h"
#include "mesh.h"
#include "collider.h"

class TerrainHeightMap
{
public:
    TerrainHeightMap(UINT width, UINT length, float cellSpacing, vector<float> heights);

    static shared_ptr<TerrainHeightMap> CreateProcedural(UINT width, UINT length, float cellSpacing, float maxHeight);
    static shared_ptr<TerrainHeightMap> CreateWaveField(UINT width, UINT length, float cellSpacing, float amplitude, float frequency);
    static shared_ptr<TerrainHeightMap> CreateSlopeTestField(UINT width, UINT length, float cellSpacing, const vector<float>& slopeAnglesDegrees);
    static shared_ptr<TerrainHeightMap> LoadRaw8(const string& filePath, UINT width, UINT length, float cellSpacing, float heightScale);
    static shared_ptr<TerrainHeightMap> LoadRawAuto(const string& filePath, float cellSpacing, float heightScale);
    static shared_ptr<TerrainHeightMap> LoadImage8(const string& filePath, float cellSpacing, float heightScale);

    UINT GetWidth() const { return m_width; }
    UINT GetLength() const { return m_length; }
    float GetCellSpacing() const { return m_cellSpacing; }
    float GetWorldWidth() const { return static_cast<float>(m_width - 1) * m_cellSpacing; }
    float GetWorldLength() const { return static_cast<float>(m_length - 1) * m_cellSpacing; }

    bool ContainsLocalXZ(float x, float z) const;
    float SampleHeight(float x, float z) const;
    XMFLOAT3 SampleNormal(float x, float z) const;
    BoundingBox GetLocalAABB() const;

private:
    float HeightAt(UINT x, UINT z) const;
    size_t Index(UINT x, UINT z) const { return static_cast<size_t>(z) * m_width + x; }

private:
    UINT m_width = 0;
    UINT m_length = 0;
    float m_cellSpacing = 1.0f;
    vector<float> m_heights;
};

class TerrainMesh final : public IndexMesh
{
private:
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT4 colors;
    };

public:
    TerrainMesh(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const shared_ptr<const TerrainHeightMap>& heightMap);
};

class TerrainCollider final : public Collider
{
public:
    explicit TerrainCollider(shared_ptr<const TerrainHeightMap> heightMap);

    void Update(const XMFLOAT4X4& worldMatrix) override;
    XMFLOAT3 GetCenter() const override { return m_worldAABB.Center; }
    BoundingBox GetWorldAABB() const override { return m_worldAABB; }
    bool Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const override;

    const shared_ptr<const TerrainHeightMap>& GetHeightMap() const { return m_heightMap; }
    const XMFLOAT4X4& GetWorldMatrix() const { return m_worldMatrix; }
    bool GetHeightAtWorld(const XMFLOAT3& worldPosition, float& outHeight, XMFLOAT3& outNormal) const;

private:
    shared_ptr<const TerrainHeightMap> m_heightMap;
    XMFLOAT4X4 m_worldMatrix{};
    XMFLOAT4X4 m_inverseWorldMatrix{};
    BoundingBox m_worldAABB{};
};
