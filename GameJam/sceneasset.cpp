#include "sceneasset.h"
#include "binaryreader.h"

#include <cctype>

namespace
{
    string StripReferencePrefix(const string& value)
    {
        if (!value.empty() && (value.front() == '#' || value.front() == '@')) return value.substr(1);
        return value;
    }

    string ToLower(string value)
    {
        transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(tolower(c));
            });
        return value;
    }

    bool ReadBool(BinaryReader& reader)
    {
        return reader.Read<uint8_t>() != 0;
    }

    XMFLOAT3 ReadFloat3(BinaryReader& reader)
    {
        return reader.Read<XMFLOAT3>();
    }

    void Expect(BinaryReader& reader, const string& expected)
    {
        string token = reader.ReadString();
        if (token != expected)
        {
            throw runtime_error("Expected token '" + expected + "', got '" + token + "' in " + reader.GetPath().string());
        }
    }

    SceneColliderShape ParseColliderShape(const string& value)
    {
        string shape = ToLower(StripReferencePrefix(value));
        if (shape == "box" || shape == "boxcollider") return SceneColliderShape::Box;
        if (shape == "capsule" || shape == "capsulecollider") return SceneColliderShape::Capsule;
        if (shape == "mesh" || shape == "meshcollider") return SceneColliderShape::Mesh;
        return SceneColliderShape::None;
    }

    bool HasPositiveSize(const XMFLOAT3& size)
    {
        return size.x > 0.0f && size.y > 0.0f && size.z > 0.0f;
    }

    void SkipMaterialValue(BinaryReader& reader, const string& token)
    {
        if (token == "<AlbedoColor>:" || token == "<EmissiveColor>:" || token == "<SpecularColor>:")
        {
            (void)reader.Read<XMFLOAT4>();
        }
        else if (token == "<Glossiness>:" || token == "<Smoothness>:" || token == "<Metallic>:" ||
            token == "<SpecularHighlight>:" || token == "<GlossyReflection>:")
        {
            (void)reader.Read<float>();
        }
        else if (token == "<AlbedoTextureName>:" || token == "<EmissionTextureName>:")
        {
            (void)reader.ReadString();
        }
        else if (token != "<Null>:")
        {
            throw runtime_error("Unknown material token: " + token);
        }
    }

    void SkipMaterials(BinaryReader& reader)
    {
        int materialCount = reader.Read<int>();
        for (int i = 0; i < materialCount; ++i)
        {
            Expect(reader, "<Material>:");
            (void)reader.ReadString();

            while (true)
            {
                string token = reader.ReadString();
                if (token == "</Material>") break;
                SkipMaterialValue(reader, token);
            }
        }

        Expect(reader, "</Materials>");
    }

    SceneColliderData ReadCollider(BinaryReader& reader)
    {
        SceneColliderData collider;
        string token = reader.ReadString();

        if (token == "<Type>:")
        {
            collider.shape = ParseColliderShape(reader.ReadString());
        }
        else if (token == "</Collider>")
        {
            return collider;
        }
        else
        {
            collider.shape = ParseColliderShape(token);
        }

        while (true)
        {
            token = reader.ReadString();
            if (token == "</Collider>") break;

            if (token == "<Type>:") collider.shape = ParseColliderShape(reader.ReadString());
            else if (token == "<Center>:" || token == "<Offset>:") collider.center = ReadFloat3(reader);
            else if (token == "<Size>:")
            {
                collider.boxSize = ReadFloat3(reader);
                collider.hasExplicitBoxSize = HasPositiveSize(collider.boxSize);
            }
            else if (token == "<Extents>:")
            {
                XMFLOAT3 extents = ReadFloat3(reader);
                collider.boxSize = XMFLOAT3{ extents.x * 2.0f, extents.y * 2.0f, extents.z * 2.0f };
                collider.hasExplicitBoxSize = HasPositiveSize(collider.boxSize);
            }
            else if (token == "<Radius>:") collider.radius = reader.Read<float>();
            else if (token == "<Height>:") collider.height = reader.Read<float>();
            else throw runtime_error("Unknown collider token: " + token);
        }

        return collider;
    }

    SceneRigidbodyData ReadRigidbody(BinaryReader& reader)
    {
        SceneRigidbodyData rigidbody;
        rigidbody.enabled = true;

        while (true)
        {
            string token = reader.ReadString();
            if (token == "</Rigidbody>") break;

            if (token == "<Mass>:") rigidbody.mass = reader.Read<float>();
            else if (token == "<Drag>:") rigidbody.drag = reader.Read<float>();
            else if (token == "<Restitution>:") rigidbody.restitution = reader.Read<float>();
            else if (token == "<IsKinematic>:" || token == "<Kinematic>:") rigidbody.isKinematic = ReadBool(reader);
            else if (token == "<UseGravity>:" || token == "<Gravity>:") rigidbody.useGravity = ReadBool(reader);
            else if (token == "<Velocity>:") rigidbody.velocity = ReadFloat3(reader);
            else throw runtime_error("Unknown rigidbody token: " + token);
        }

        return rigidbody;
    }

    SceneObjectData ReadGameObject(BinaryReader& reader)
    {
        SceneObjectData object;
        XMStoreFloat4x4(&object.worldMatrix, XMMatrixIdentity());

        while (true)
        {
            string token = reader.ReadString();

            if (token == "<GameObject>:")
            {
                object.name = reader.ReadString();
                reader.ReadInto(object.worldMatrix);
            }
            else if (token == "<Mesh>:")
            {
                object.meshName = StripReferencePrefix(reader.ReadString());
                Expect(reader, "</Mesh>");
            }
            else if (token == "<Materials>:")
            {
                SkipMaterials(reader);
            }
            else if (token == "<Collider>:")
            {
                object.collider = ReadCollider(reader);
            }
            else if (token == "<Rigidbody>:")
            {
                object.rigidbody = ReadRigidbody(reader);
            }
            else if (token == "</GameObject>")
            {
                break;
            }
            else
            {
                throw runtime_error("Unknown game object token: " + token);
            }
        }

        return object;
    }

    class SceneMeshCache
    {
    public:
        shared_ptr<Mesh> GetOrCreate(const string& meshName,
            const filesystem::path& sceneDirectory,
            const ComPtr<ID3D12Device>& device,
            const ComPtr<ID3D12GraphicsCommandList>& commandList)
        {
            if (meshName.empty()) return nullptr;

            auto found = m_meshes.find(meshName);
            if (found != m_meshes.end()) return found->second;

            filesystem::path meshFile(meshName);
            meshFile.replace_extension(".bin");
            filesystem::path meshPath = sceneDirectory / meshFile;
            auto mesh = make_shared<BinaryMesh>(device, commandList, meshPath.string());
            m_meshes.emplace(meshName, mesh);
            return mesh;
        }

        vector<shared_ptr<Mesh>> GetLoadedMeshes() const
        {
            vector<shared_ptr<Mesh>> meshes;
            meshes.reserve(m_meshes.size());
            for (const auto& [name, mesh] : m_meshes)
            {
                (void)name;
                meshes.push_back(mesh);
            }
            return meshes;
        }

    private:
        unordered_map<string, shared_ptr<Mesh>> m_meshes;
    };
}

SceneAsset UnitySceneBinaryReader::Load(const filesystem::path& scenePath) const
{
    BinaryReader reader(scenePath);

    SceneAsset asset;
    asset.sourcePath = scenePath;

    Expect(reader, "<Meshes>:");
    int meshCount = reader.Read<int>();
    asset.meshNames.reserve(max(meshCount, 0));
    for (int i = 0; i < meshCount; ++i) asset.meshNames.push_back(reader.ReadString());

    Expect(reader, "<Materials>:");
    int materialCount = reader.Read<int>();
    asset.materialNames.reserve(max(materialCount, 0));
    for (int i = 0; i < materialCount; ++i) asset.materialNames.push_back(reader.ReadString());

    Expect(reader, "<GameObjects>:");
    int objectCount = reader.Read<int>();
    asset.objects.reserve(max(objectCount, 0));
    for (int i = 0; i < objectCount; ++i) asset.objects.push_back(ReadGameObject(reader));

    Expect(reader, "</GameObjects>");
    return asset;
}

shared_ptr<GameObject> SceneObjectFactory::Create(const SceneObjectData& data, const shared_ptr<Mesh>& mesh) const
{
    auto object = make_shared<GameObject>();
    object->SetName(data.name);
    object->SetMeshName(data.meshName);
    object->SetMesh(mesh);
    object->SetWorldMatrix(data.worldMatrix);

    shared_ptr<Collider> collider;
    switch (data.collider.shape)
    {
    case SceneColliderShape::Box:
        collider = data.collider.hasExplicitBoxSize
            ? make_shared<BoxCollider>(data.collider.center, data.collider.boxSize)
            : make_shared<BoxCollider>(mesh);
        break;
    case SceneColliderShape::Capsule:
        collider = make_shared<CapsuleCollider>(data.collider.radius, data.collider.height, data.collider.center);
        break;
    case SceneColliderShape::Mesh:
        collider = make_shared<MeshCollider>(mesh);
        break;
    default:
        break;
    }

    if (collider) object->SetCollider(collider);

    if (data.rigidbody.enabled)
    {
        auto rigidbody = make_shared<Rigidbody>();
        rigidbody->SetMass(data.rigidbody.mass);
        rigidbody->SetDrag(data.rigidbody.drag);
        rigidbody->SetRestitution(data.rigidbody.restitution);
        rigidbody->SetKinematic(data.rigidbody.isKinematic);
        rigidbody->SetUseGravity(data.rigidbody.useGravity);
        rigidbody->SetVelocity(data.rigidbody.velocity);
        object->SetRigidbody(rigidbody);
    }

    return object;
}

SceneBuildResult BinarySceneInstantiator::Build(const filesystem::path& scenePath,
    const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    CollisionManager* collisionManager,
    PhysicsManager* physicsManager) const
{
    UnitySceneBinaryReader reader;
    SceneAsset asset = reader.Load(scenePath);

    SceneMeshCache meshCache;
    SceneObjectFactory objectFactory;
    SceneBuildResult result;
    result.objects.reserve(asset.objects.size());

    filesystem::path sceneDirectory = scenePath.parent_path();
    for (const auto& objectData : asset.objects)
    {
        auto mesh = meshCache.GetOrCreate(objectData.meshName, sceneDirectory, device, commandList);
        auto object = objectFactory.Create(objectData, mesh);

        if (collisionManager && object->GetCollider()) collisionManager->AddCollider(object->GetCollider());
        if (physicsManager && object->GetRigidbody()) physicsManager->AddRigidbody(object->GetRigidbody());

        result.objects.push_back(object);
    }

    result.loadedMeshes = meshCache.GetLoadedMeshes();
    return result;
}
