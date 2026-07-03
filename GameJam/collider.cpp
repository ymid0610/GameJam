#include "collider.h"

using namespace Utiles::Physics;

namespace
{
    bool IntersectRaySphere(FXMVECTOR origin, FXMVECTOR direction, FXMVECTOR center, float radius, float& outDist)
    {
        XMVECTOR toOrigin = XMVectorSubtract(origin, center);
        float b = VectorDot(toOrigin, direction);
        float c = VectorDot(toOrigin, toOrigin) - radius * radius;

        if (c > 0.0f && b > 0.0f) return false;

        float discriminant = b * b - c;
        if (discriminant < 0.0f) return false;

        outDist = -b - sqrtf(discriminant);
        if (outDist < 0.0f) outDist = 0.0f;
        return true;
    }

    bool IntersectRayTriangle(FXMVECTOR origin, FXMVECTOR direction,
        FXMVECTOR v0, FXMVECTOR v1, FXMVECTOR v2, float& outDist)
    {
        constexpr float RayEpsilon = 1.0e-6f;

        XMVECTOR edge1 = XMVectorSubtract(v1, v0);
        XMVECTOR edge2 = XMVectorSubtract(v2, v0);
        XMVECTOR p = XMVector3Cross(direction, edge2);
        float det = VectorDot(edge1, p);

        if (fabsf(det) <= RayEpsilon) return false;

        float invDet = 1.0f / det;
        XMVECTOR t = XMVectorSubtract(origin, v0);
        float u = VectorDot(t, p) * invDet;
        if (u < 0.0f || u > 1.0f) return false;

        XMVECTOR q = XMVector3Cross(t, edge1);
        float v = VectorDot(direction, q) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;

        float distance = VectorDot(edge2, q) * invDet;
        if (distance < 0.0f) return false;

        outDist = distance;
        return true;
    }

    XMFLOAT3 TransformPoint(const XMFLOAT3& point, CXMMATRIX matrix)
    {
        XMFLOAT3 transformed{};
        XMStoreFloat3(&transformed, XMVector3TransformCoord(XMLoadFloat3(&point), matrix));
        return transformed;
    }

    bool StartsWithName(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
    }

    bool IsRocketPartName(const string& name)
    {
        return name == "rocket" ||
            StartsWithName(name, "rocket_") ||
            StartsWithName(name, "rocket:") ||
            StartsWithName(name, "rocket#");
    }

    bool IsRocketGroupBox(const string& name, const BoundingOrientedBox& localOBB)
    {
        if (!IsRocketPartName(name)) return false;

        const float width = localOBB.Extents.x * 2.0f;
        const float height = localOBB.Extents.y * 2.0f;
        const float depth = localOBB.Extents.z * 2.0f;
        const float longest = max(width, max(height, depth));
        const float shortest = min(width, min(height, depth));

        return longest > 14.0f || shortest > 3.0f;
    }

}

bool BoxCollider::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const
{
    XMVECTOR rayOrigin = XMLoadFloat3(&origin);
    XMVECTOR rayDirection = SafeNormalize(XMLoadFloat3(&direction), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

    return m_worldOBB.Intersects(rayOrigin, rayDirection, outDist);
}

void CapsuleCollider::Update(const XMFLOAT4X4& worldMatrix)
{
    XMStoreFloat3(&m_worldCenter,
        XMVector3TransformCoord(XMLoadFloat3(&m_localCenter), XMLoadFloat4x4(&worldMatrix)));

    XMVECTOR up = XMVectorSet(worldMatrix._21, worldMatrix._22, worldMatrix._23, 0.0f);
    up = SafeNormalize(up, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    XMVECTOR pos = XMLoadFloat3(&m_worldCenter);
    XMVECTOR halfHeight = XMVectorScale(up, m_height * 0.5f);

    XMStoreFloat3(&m_worldPointA, XMVectorAdd(pos, halfHeight));
    XMStoreFloat3(&m_worldPointB, XMVectorSubtract(pos, halfHeight));

    XMFLOAT3 minPoint{
        min(m_worldPointA.x, m_worldPointB.x) - m_radius,
        min(m_worldPointA.y, m_worldPointB.y) - m_radius,
        min(m_worldPointA.z, m_worldPointB.z) - m_radius
    };
    XMFLOAT3 maxPoint{
        max(m_worldPointA.x, m_worldPointB.x) + m_radius,
        max(m_worldPointA.y, m_worldPointB.y) + m_radius,
        max(m_worldPointA.z, m_worldPointB.z) + m_radius
    };

    BoundingBox::CreateFromPoints(m_worldAABB, XMLoadFloat3(&minPoint), XMLoadFloat3(&maxPoint));
}

bool CapsuleCollider::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const
{
    XMVECTOR rayOrigin = XMLoadFloat3(&origin);
    XMVECTOR rayDirection = SafeNormalize(XMLoadFloat3(&direction), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
    XMVECTOR segmentA = XMLoadFloat3(&m_worldPointA);
    XMVECTOR segmentB = XMLoadFloat3(&m_worldPointB);
    XMVECTOR capsuleAxis = XMVectorSubtract(segmentB, segmentA);

    XMVECTOR originClosest = ClosestPointOnSegment(rayOrigin, segmentA, segmentB);
    if (VectorLengthSq(XMVectorSubtract(rayOrigin, originClosest)) <= m_radius * m_radius)
    {
        outDist = 0.0f;
        return true;
    }

    float nearestHit = FLT_MAX;
    bool hit = false;

    float sphereHit = 0.0f;
    if (IntersectRaySphere(rayOrigin, rayDirection, segmentA, m_radius, sphereHit))
    {
        nearestHit = min(nearestHit, sphereHit);
        hit = true;
    }
    if (IntersectRaySphere(rayOrigin, rayDirection, segmentB, m_radius, sphereHit))
    {
        nearestHit = min(nearestHit, sphereHit);
        hit = true;
    }

    float axisLengthSq = VectorLengthSq(capsuleAxis);
    if (axisLengthSq > Epsilon)
    {
        XMVECTOR originToSegment = XMVectorSubtract(rayOrigin, segmentA);
        float axisDotDirection = VectorDot(capsuleAxis, rayDirection);
        float axisDotOrigin = VectorDot(capsuleAxis, originToSegment);
        float directionDotOrigin = VectorDot(rayDirection, originToSegment);
        float originLengthSq = VectorDot(originToSegment, originToSegment);

        float a = axisLengthSq - axisDotDirection * axisDotDirection;
        float b = axisLengthSq * directionDotOrigin - axisDotOrigin * axisDotDirection;
        float c = axisLengthSq * originLengthSq - axisDotOrigin * axisDotOrigin - m_radius * m_radius * axisLengthSq;

        if (fabsf(a) > Epsilon)
        {
            float discriminant = b * b - a * c;
            if (discriminant >= 0.0f)
            {
                float cylinderHit = (-b - sqrtf(discriminant)) / a;
                float capsuleAxisProjection = axisDotOrigin + cylinderHit * axisDotDirection;

                if (cylinderHit >= 0.0f && capsuleAxisProjection >= 0.0f && capsuleAxisProjection <= axisLengthSq)
                {
                    nearestHit = min(nearestHit, cylinderHit);
                    hit = true;
                }
            }
        }
    }

    if (!hit) return false;

    outDist = nearestHit;
    return true;
}

MeshCollider::MeshCollider(const shared_ptr<Mesh>& mesh)
    : Collider(ColliderType::Mesh)
{
    if (mesh)
    {
        m_localTriangles = mesh->GetLocalTriangles();
        m_localOBB = mesh->GetLocalOBB();
    }
}

void MeshCollider::Update(const XMFLOAT4X4& worldMatrix)
{
    m_worldTriangles.clear();
    m_worldTriangles.reserve(m_localTriangles.size());
    m_localOBB.Transform(m_worldOBB, XMLoadFloat4x4(&worldMatrix));

    vector<XMFLOAT3> worldPoints;
    worldPoints.reserve(m_localTriangles.size() * 3);

    XMMATRIX world = XMLoadFloat4x4(&worldMatrix);
    for (const auto& triangle : m_localTriangles)
    {
        MeshTriangle worldTriangle{
            TransformPoint(triangle.a, world),
            TransformPoint(triangle.b, world),
            TransformPoint(triangle.c, world)
        };

        m_worldTriangles.push_back(worldTriangle);
        worldPoints.push_back(worldTriangle.a);
        worldPoints.push_back(worldTriangle.b);
        worldPoints.push_back(worldTriangle.c);
    }

    if (worldPoints.empty())
    {
        m_worldAABB = BoundingBox{};
        return;
    }

    BoundingBox::CreateFromPoints(m_worldAABB, worldPoints.size(), worldPoints.data(), sizeof(XMFLOAT3));
}

bool MeshCollider::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const
{
    XMVECTOR rayOrigin = XMLoadFloat3(&origin);
    XMVECTOR rayDirection = SafeNormalize(XMLoadFloat3(&direction), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

    float nearestHit = FLT_MAX;
    bool hit = false;

    for (const auto& triangle : m_worldTriangles)
    {
        float hitDist = 0.0f;
        if (IntersectRayTriangle(rayOrigin, rayDirection,
            XMLoadFloat3(&triangle.a), XMLoadFloat3(&triangle.b), XMLoadFloat3(&triangle.c), hitDist) &&
            hitDist < nearestHit)
        {
            nearestHit = hitDist;
            hit = true;
        }
    }

    if (!hit) return false;

    outDist = nearestHit;
    return true;
}

CompoundCollider::CompoundCollider()
    : Collider(ColliderType::Compound)
{
}

CompoundCollider::CompoundCollider(const shared_ptr<Mesh>& mesh, float padding)
    : Collider(ColliderType::Compound)
{
    AddBoxesFromMeshParts(mesh, padding);
}

void CompoundCollider::SetOwner(const shared_ptr<GameObject>& owner)
{
    Collider::SetOwner(owner);

    for (const auto& child : m_children)
    {
        if (child.collider) child.collider->SetOwner(owner);
    }
}

void CompoundCollider::Update(const XMFLOAT4X4& worldMatrix)
{
    for (const auto& child : m_children)
    {
        if (child.collider) child.collider->Update(worldMatrix);
    }

    RebuildWorldAABB();
}

bool CompoundCollider::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outDist) const
{
    float nearestHit = FLT_MAX;
    bool hit = false;

    for (const auto& child : m_children)
    {
        if (!child.collider) continue;

        float childHit = 0.0f;
        if (child.collider->Raycast(origin, direction, childHit) && childHit < nearestHit)
        {
            nearestHit = childHit;
            hit = true;
        }
    }

    if (!hit) return false;

    outDist = nearestHit;
    return true;
}

void CompoundCollider::AddCollider(const string& name, const shared_ptr<Collider>& collider)
{
    if (!collider) return;

    if (auto owner = m_owner.lock()) collider->SetOwner(owner);
    m_children.push_back({ name, collider });
    RebuildWorldAABB();
}

void CompoundCollider::AddBox(const string& name, XMFLOAT3 center, XMFLOAT3 size)
{
    AddCollider(name, make_shared<BoxCollider>(center, size));
}

void CompoundCollider::AddBox(const string& name, BoundingOrientedBox localOBB)
{
    AddCollider(name, make_shared<BoxCollider>(localOBB));
}

void CompoundCollider::AddBoxesFromMeshParts(const shared_ptr<Mesh>& mesh, float padding)
{
    if (!mesh) return;

    const float extraExtent = max(padding, 0.0f);
    const auto& parts = mesh->GetLocalParts();
    for (const auto& part : parts)
    {
        BoundingOrientedBox localOBB = part.localOBB;
        localOBB.Extents = XMFLOAT3{
            localOBB.Extents.x + extraExtent,
            localOBB.Extents.y + extraExtent,
            localOBB.Extents.z + extraExtent
        };

        if (IsRocketGroupBox(part.name, localOBB)) continue;

        AddBox(part.name, localOBB);
    }

    if (!m_children.empty()) return;
    if (dynamic_pointer_cast<FbxStaticMesh>(mesh)) return;

    BoundingOrientedBox bounds = mesh->GetLocalOBB();
    bounds.Extents = XMFLOAT3{
        bounds.Extents.x + extraExtent,
        bounds.Extents.y + extraExtent,
        bounds.Extents.z + extraExtent
    };
    AddBox("Mesh", bounds);
}

void CompoundCollider::RebuildWorldAABB()
{
    bool hasBounds = false;
    XMFLOAT3 minPoint{ FLT_MAX, FLT_MAX, FLT_MAX };
    XMFLOAT3 maxPoint{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (const auto& child : m_children)
    {
        if (!child.collider) continue;

        BoundingBox childBounds = child.collider->GetWorldAABB();
        XMFLOAT3 childMin{
            childBounds.Center.x - childBounds.Extents.x,
            childBounds.Center.y - childBounds.Extents.y,
            childBounds.Center.z - childBounds.Extents.z
        };
        XMFLOAT3 childMax{
            childBounds.Center.x + childBounds.Extents.x,
            childBounds.Center.y + childBounds.Extents.y,
            childBounds.Center.z + childBounds.Extents.z
        };

        minPoint.x = min(minPoint.x, childMin.x);
        minPoint.y = min(minPoint.y, childMin.y);
        minPoint.z = min(minPoint.z, childMin.z);
        maxPoint.x = max(maxPoint.x, childMax.x);
        maxPoint.y = max(maxPoint.y, childMax.y);
        maxPoint.z = max(maxPoint.z, childMax.z);
        hasBounds = true;
    }

    if (!hasBounds)
    {
        m_worldAABB = BoundingBox{};
        return;
    }

    BoundingBox::CreateFromPoints(m_worldAABB, XMLoadFloat3(&minPoint), XMLoadFloat3(&maxPoint));
}
