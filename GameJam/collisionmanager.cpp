#include "collisionmanager.h"
#include "object.h"
#include "terrain.h"

using namespace Utiles::Physics;

namespace
{
    struct CollisionCandidate
    {
        shared_ptr<Collider> colliderA;
        shared_ptr<Collider> colliderB;
        shared_ptr<GameObject> ownerA;
        shared_ptr<GameObject> ownerB;
    };

    struct ColliderProxy
    {
        shared_ptr<Collider> collider;
        shared_ptr<GameObject> owner;
    };

    bool IsDynamicObject(const shared_ptr<GameObject>& object)
    {
        return object && object->GetInverseMass() > 0.0f;
    }

    bool IsCharacterObject(const shared_ptr<GameObject>& object)
    {
        return object && object->IsCharacterController();
    }

    bool BroadPhaseOverlap(const shared_ptr<Collider>& colA, const shared_ptr<Collider>& colB)
    {
        BoundingBox aabbA = colA->GetWorldAABB();
        BoundingBox aabbB = colB->GetWorldAABB();
        return aabbA.Intersects(aabbB);
    }

    void AddCandidateIfBroadPhaseOverlaps(vector<CollisionCandidate>& candidates,
        const ColliderProxy& proxyA, const ColliderProxy& proxyB)
    {
        if (!BroadPhaseOverlap(proxyA.collider, proxyB.collider)) return;
        candidates.push_back({ proxyA.collider, proxyB.collider, proxyA.owner, proxyB.owner });
    }

    vector<CollisionCandidate> BuildCollisionCandidates(const vector<shared_ptr<Collider>>& colliders)
    {
        vector<ColliderProxy> dynamicColliders;
        vector<ColliderProxy> staticColliders;
        dynamicColliders.reserve(colliders.size());
        staticColliders.reserve(colliders.size());

        for (const auto& collider : colliders)
        {
            if (!collider) continue;

            auto owner = collider->m_owner.lock();
            if (!owner) continue;

            ColliderProxy proxy{ collider, owner };
            if (IsDynamicObject(owner)) dynamicColliders.push_back(proxy);
            else staticColliders.push_back(proxy);
        }

        vector<CollisionCandidate> candidates;
        candidates.reserve(dynamicColliders.size() * 2);

        for (size_t i = 0; i < dynamicColliders.size(); ++i)
        {
            const auto& dynamicA = dynamicColliders[i];

            for (size_t j = i + 1; j < dynamicColliders.size(); ++j)
            {
                AddCandidateIfBroadPhaseOverlaps(candidates, dynamicA, dynamicColliders[j]);
            }

            for (const auto& staticCollider : staticColliders)
            {
                AddCandidateIfBroadPhaseOverlaps(candidates, dynamicA, staticCollider);
            }
        }

        return candidates;
    }

    bool ComputeOBBOBBContact(const BoundingOrientedBox& obbA, const BoundingOrientedBox& obbB, ContactInfo& outContact)
    {
        XMVECTOR axesA[3]{};
        XMVECTOR axesB[3]{};
        GetOBBAxes(obbA, axesA);
        GetOBBAxes(obbB, axesB);

        XMVECTOR centerA = XMLoadFloat3(&obbA.Center);
        XMVECTOR centerB = XMLoadFloat3(&obbB.Center);
        XMVECTOR centerDelta = XMVectorSubtract(centerB, centerA);

        float minOverlap = FLT_MAX;
        XMVECTOR minAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        for (int i = 0; i < 3; ++i)
        {
            if (!TestOBBAxis(obbA, axesA, obbB, axesB, centerDelta, axesA[i], minOverlap, minAxis)) return false;
            if (!TestOBBAxis(obbA, axesA, obbB, axesB, centerDelta, axesB[i], minOverlap, minAxis)) return false;
        }

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                XMVECTOR crossAxis = XMVector3Cross(axesA[i], axesB[j]);
                if (!TestOBBAxis(obbA, axesA, obbB, axesB, centerDelta, crossAxis, minOverlap, minAxis)) return false;
            }
        }

        outContact.normal = ToFloat3(SafeNormalize(minAxis, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
        outContact.penetration = max(minOverlap, 0.0f);
        return true;
    }

    void ProjectTriangle(FXMVECTOR a, FXMVECTOR b, FXMVECTOR c, FXMVECTOR axis, float& outMin, float& outMax)
    {
        float projectionA = VectorDot(a, axis);
        float projectionB = VectorDot(b, axis);
        float projectionC = VectorDot(c, axis);

        outMin = min(projectionA, min(projectionB, projectionC));
        outMax = max(projectionA, max(projectionB, projectionC));
    }

    bool TestOBBTriangleAxis(const BoundingOrientedBox& obb, const XMVECTOR obbAxes[3],
        FXMVECTOR triangleA, FXMVECTOR triangleB, FXMVECTOR triangleC, FXMVECTOR triangleCenter,
        FXMVECTOR axis, float& minOverlap, XMVECTOR& minAxis)
    {
        if (VectorLengthSq(axis) <= AxisEpsilon) return true;

        XMVECTOR normal = XMVector3Normalize(axis);
        XMVECTOR boxCenter = XMLoadFloat3(&obb.Center);

        float boxProjection = VectorDot(boxCenter, normal);
        float boxRadius = ProjectOBBRadius(obb, obbAxes, normal);
        float boxMin = boxProjection - boxRadius;
        float boxMax = boxProjection + boxRadius;

        float triangleMin = 0.0f;
        float triangleMax = 0.0f;
        ProjectTriangle(triangleA, triangleB, triangleC, normal, triangleMin, triangleMax);

        float overlap = min(boxMax, triangleMax) - max(boxMin, triangleMin);
        if (overlap < 0.0f) return false;

        if (overlap < minOverlap)
        {
            minOverlap = overlap;
            float direction = VectorDot(XMVectorSubtract(triangleCenter, boxCenter), normal) >= 0.0f ? 1.0f : -1.0f;
            minAxis = XMVectorScale(normal, direction);
        }

        return true;
    }

    bool ComputeOBBTriangleContact(const BoundingOrientedBox& obb,
        FXMVECTOR triangleA, FXMVECTOR triangleB, FXMVECTOR triangleC, ContactInfo& outContact)
    {
        XMVECTOR obbAxes[3]{};
        GetOBBAxes(obb, obbAxes);

        XMVECTOR edge0 = XMVectorSubtract(triangleB, triangleA);
        XMVECTOR edge1 = XMVectorSubtract(triangleC, triangleB);
        XMVECTOR edge2 = XMVectorSubtract(triangleA, triangleC);
        XMVECTOR triangleNormal = XMVector3Cross(edge0, XMVectorSubtract(triangleC, triangleA));
        if (VectorLengthSq(triangleNormal) <= Epsilon) return false;

        XMVECTOR triangleCenter = XMVectorScale(XMVectorAdd(XMVectorAdd(triangleA, triangleB), triangleC), 1.0f / 3.0f);
        float minOverlap = FLT_MAX;
        XMVECTOR minAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        for (int i = 0; i < 3; ++i)
        {
            if (!TestOBBTriangleAxis(obb, obbAxes, triangleA, triangleB, triangleC, triangleCenter,
                obbAxes[i], minOverlap, minAxis)) return false;
        }

        if (!TestOBBTriangleAxis(obb, obbAxes, triangleA, triangleB, triangleC, triangleCenter,
            triangleNormal, minOverlap, minAxis)) return false;

        XMVECTOR edges[3] = { edge0, edge1, edge2 };
        for (const auto& edge : edges)
        {
            for (int i = 0; i < 3; ++i)
            {
                XMVECTOR axis = XMVector3Cross(edge, obbAxes[i]);
                if (!TestOBBTriangleAxis(obb, obbAxes, triangleA, triangleB, triangleC, triangleCenter,
                    axis, minOverlap, minAxis)) return false;
            }
        }

        outContact.normal = ToFloat3(SafeNormalize(minAxis, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
        outContact.penetration = max(minOverlap, 0.0f);
        return true;
    }

    bool ComputeOBBMeshContact(const BoundingOrientedBox& obb, const MeshCollider& mesh, ContactInfo& outContact)
    {
        return ComputeOBBOBBContact(obb, mesh.GetWorldOBB(), outContact);
    }

    float GetExtentByAxisIndex(const XMFLOAT3& extents, int axis)
    {
        if (axis == 0) return extents.x;
        if (axis == 1) return extents.y;
        return extents.z;
    }

    float GetSlopeAngleDegrees(const XMFLOAT3& normal)
    {
        float upDot = clamp(normal.y, -1.0f, 1.0f);
        return XMConvertToDegrees(acosf(upDot));
    }

    XMFLOAT3 GetTerrainPushDirection(const XMFLOAT3& terrainNormal, bool isWalkable, bool limitSteepSlope)
    {
        if (isWalkable || !limitSteepSlope) return terrainNormal;

        XMVECTOR lateral = XMVectorSet(terrainNormal.x, 0.0f, terrainNormal.z, 0.0f);
        lateral = SafeNormalize(lateral, XMLoadFloat3(&terrainNormal));
        return ToFloat3(lateral);
    }

    int BuildOBBBottomFaceSamples(const BoundingOrientedBox& obb, XMFLOAT3 samples[9])
    {
        XMVECTOR axes[3]{};
        GetOBBAxes(obb, axes);

        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        int verticalAxis = 0;
        float bestUpAlignment = -1.0f;
        for (int i = 0; i < 3; ++i)
        {
            float alignment = fabsf(VectorDot(axes[i], up));
            if (alignment > bestUpAlignment)
            {
                bestUpAlignment = alignment;
                verticalAxis = i;
            }
        }

        int tangentAxes[2]{};
        int tangentCount = 0;
        for (int i = 0; i < 3; ++i)
        {
            if (i != verticalAxis) tangentAxes[tangentCount++] = i;
        }

        float verticalDot = VectorDot(axes[verticalAxis], up);
        XMVECTOR bottomDirection = verticalDot >= 0.0f ? XMVectorNegate(axes[verticalAxis]) : axes[verticalAxis];
        XMVECTOR center = XMLoadFloat3(&obb.Center);
        XMVECTOR bottomCenter = XMVectorAdd(center,
            XMVectorScale(bottomDirection, GetExtentByAxisIndex(obb.Extents, verticalAxis)));

        int count = 0;
        for (int u = -1; u <= 1; ++u)
        {
            for (int v = -1; v <= 1; ++v)
            {
                XMVECTOR sample = bottomCenter;
                sample = XMVectorAdd(sample,
                    XMVectorScale(axes[tangentAxes[0]], GetExtentByAxisIndex(obb.Extents, tangentAxes[0]) * static_cast<float>(u)));
                sample = XMVectorAdd(sample,
                    XMVectorScale(axes[tangentAxes[1]], GetExtentByAxisIndex(obb.Extents, tangentAxes[1]) * static_cast<float>(v)));
                XMStoreFloat3(&samples[count++], sample);
            }
        }

        return count;
    }

    bool ComputeOBBTerrainContact(const BoundingOrientedBox& obb, const TerrainCollider& terrain,
        bool limitSteepSlope, ContactInfo& outContact)
    {
        XMFLOAT3 samples[9]{};
        int sampleCount = BuildOBBBottomFaceSamples(obb, samples);

        auto findContact = [&](bool requireWalkable, ContactInfo& contact)
        {
            bool hit = false;
            float deepestPenetration = 0.0f;
            XMFLOAT3 bestNormal{ 0.0f, 1.0f, 0.0f };
            bool bestWalkable = true;
            float bestSlopeAngle = 0.0f;

            for (int i = 0; i < sampleCount; ++i)
            {
                float terrainHeight = 0.0f;
                XMFLOAT3 terrainNormal{};
                if (!terrain.GetHeightAtWorld(samples[i], terrainHeight, terrainNormal)) continue;

                float penetration = terrainHeight - samples[i].y;
                if (penetration <= deepestPenetration) continue;

                bool walkable = terrainNormal.y >= MaxWalkableSlopeCos;
                if (requireWalkable && !walkable) continue;

                deepestPenetration = penetration;
                bestNormal = terrainNormal;
                bestWalkable = walkable;
                bestSlopeAngle = GetSlopeAngleDegrees(terrainNormal);
                hit = penetration > 0.0f;
            }

            if (!hit) return false;

            XMFLOAT3 pushDirection = GetTerrainPushDirection(bestNormal, bestWalkable, limitSteepSlope);
            contact.normal = Utiles::Vector3::Mul(pushDirection, -1.0f);
            contact.surfaceNormal = bestNormal;
            contact.penetration = deepestPenetration;
            contact.isTerrainContact = true;
            contact.isWalkable = bestWalkable;
            contact.slopeAngleDegrees = bestSlopeAngle;
            return true;
        };

        return findContact(true, outContact) || findContact(false, outContact);
    }

    bool ComputeCapsuleTerrainContact(const CapsuleCollider& capsule, const TerrainCollider& terrain,
        bool limitSteepSlope, ContactInfo& outContact)
    {
        XMFLOAT3 pointA = capsule.GetPointA();
        XMFLOAT3 pointB = capsule.GetPointB();
        XMFLOAT3 bottomPoint = pointA.y < pointB.y ? pointA : pointB;

        float terrainHeight = 0.0f;
        XMFLOAT3 terrainNormal{};
        if (!terrain.GetHeightAtWorld(bottomPoint, terrainHeight, terrainNormal)) return false;

        float capsuleBottom = bottomPoint.y - capsule.GetRadius();
        float penetration = terrainHeight - capsuleBottom;
        if (penetration <= 0.0f) return false;

        bool walkable = terrainNormal.y >= MaxWalkableSlopeCos;
        XMFLOAT3 pushDirection = GetTerrainPushDirection(terrainNormal, walkable, limitSteepSlope);
        outContact.normal = Utiles::Vector3::Mul(pushDirection, -1.0f);
        outContact.surfaceNormal = terrainNormal;
        outContact.penetration = penetration;
        outContact.isTerrainContact = true;
        outContact.isWalkable = walkable;
        outContact.slopeAngleDegrees = GetSlopeAngleDegrees(terrainNormal);
        return true;
    }

    bool ComputeCapsuleOBBContact(const CapsuleCollider& capsule, const BoundingOrientedBox& obb, ContactInfo& outContact)
    {
        XMFLOAT3 pointA = capsule.GetPointA();
        XMFLOAT3 pointB = capsule.GetPointB();
        XMFLOAT3 capsuleCenterFloat = capsule.GetCenter();

        XMVECTOR segmentA = XMLoadFloat3(&pointA);
        XMVECTOR segmentB = XMLoadFloat3(&pointB);
        XMVECTOR capsuleCenter = XMLoadFloat3(&capsuleCenterFloat);
        XMVECTOR boxCenter = XMLoadFloat3(&obb.Center);

        XMVECTOR axes[3]{};
        GetOBBAxes(obb, axes);

        XMVECTOR segmentPoint = ClosestPointOnSegment(boxCenter, segmentA, segmentB);
        XMVECTOR boxPoint = ClosestPointOnOBB(segmentPoint, obb, axes);

        for (int i = 0; i < 4; ++i)
        {
            segmentPoint = ClosestPointOnSegment(boxPoint, segmentA, segmentB);
            boxPoint = ClosestPointOnOBB(segmentPoint, obb, axes);
        }

        XMVECTOR capToBox = XMVectorSubtract(boxPoint, segmentPoint);
        float distanceSq = VectorLengthSq(capToBox);
        float radius = capsule.GetRadius();
        float radiusSq = radius * radius;

        if (distanceSq > radiusSq) return false;

        XMVECTOR normal{};
        float penetration = 0.0f;

        if (distanceSq > Epsilon)
        {
            float distance = sqrtf(distanceSq);
            normal = XMVectorScale(capToBox, 1.0f / distance);
            penetration = radius - distance;
        }
        else
        {
            XMFLOAT3 localPoint{};
            XMFLOAT3 localCenter{};
            XMStoreFloat3(&localPoint, GetOBBLocalCoordinates(segmentPoint, obb, axes));
            XMStoreFloat3(&localCenter, GetOBBLocalCoordinates(capsuleCenter, obb, axes));

            float coords[3] = { localPoint.x, localPoint.y, localPoint.z };
            float centerCoords[3] = { localCenter.x, localCenter.y, localCenter.z };
            float extents[3] = { obb.Extents.x, obb.Extents.y, obb.Extents.z };

            int bestAxis = 0;
            float bestDistance = FLT_MAX;
            float bestSign = -1.0f;

            for (int i = 0; i < 3; ++i)
            {
                float faceDistance = max(extents[i] - fabsf(coords[i]), 0.0f);
                if (faceDistance < bestDistance)
                {
                    bestAxis = i;
                    bestDistance = faceDistance;

                    float reference = fabsf(centerCoords[i]) > Epsilon ? centerCoords[i] : coords[i];
                    bestSign = reference >= 0.0f ? -1.0f : 1.0f;
                }
            }

            normal = XMVectorScale(axes[bestAxis], bestSign);
            penetration = radius + bestDistance;
        }

        outContact.normal = ToFloat3(SafeNormalize(normal, XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f)));
        outContact.penetration = max(penetration, 0.0f);
        return true;
    }

    void ClosestPointsBetweenSegments(FXMVECTOR p1, FXMVECTOR q1, FXMVECTOR p2, FXMVECTOR q2,
        XMVECTOR& outPoint1, XMVECTOR& outPoint2)
    {
        XMVECTOR d1 = XMVectorSubtract(q1, p1);
        XMVECTOR d2 = XMVectorSubtract(q2, p2);
        XMVECTOR r = XMVectorSubtract(p1, p2);

        float a = VectorDot(d1, d1);
        float e = VectorDot(d2, d2);
        float f = VectorDot(d2, r);

        float s = 0.0f;
        float t = 0.0f;

        if (a <= Epsilon && e <= Epsilon)
        {
            outPoint1 = p1;
            outPoint2 = p2;
            return;
        }

        if (a <= Epsilon)
        {
            t = clamp(f / e, 0.0f, 1.0f);
        }
        else
        {
            float c = VectorDot(d1, r);

            if (e <= Epsilon)
            {
                s = clamp(-c / a, 0.0f, 1.0f);
            }
            else
            {
                float b = VectorDot(d1, d2);
                float denom = a * e - b * b;

                if (fabsf(denom) > Epsilon) s = clamp((b * f - c * e) / denom, 0.0f, 1.0f);

                float tNumerator = b * s + f;
                if (tNumerator < 0.0f)
                {
                    t = 0.0f;
                    s = clamp(-c / a, 0.0f, 1.0f);
                }
                else if (tNumerator > e)
                {
                    t = 1.0f;
                    s = clamp((b - c) / a, 0.0f, 1.0f);
                }
                else
                {
                    t = tNumerator / e;
                }
            }
        }

        outPoint1 = XMVectorAdd(p1, XMVectorScale(d1, s));
        outPoint2 = XMVectorAdd(p2, XMVectorScale(d2, t));
    }

    XMVECTOR ClosestPointOnTriangle(FXMVECTOR point, FXMVECTOR a, FXMVECTOR b, FXMVECTOR c)
    {
        XMVECTOR ab = XMVectorSubtract(b, a);
        XMVECTOR ac = XMVectorSubtract(c, a);
        XMVECTOR ap = XMVectorSubtract(point, a);

        float d1 = VectorDot(ab, ap);
        float d2 = VectorDot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        XMVECTOR bp = XMVectorSubtract(point, b);
        float d3 = VectorDot(ab, bp);
        float d4 = VectorDot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        {
            float v = d1 / (d1 - d3);
            return XMVectorAdd(a, XMVectorScale(ab, v));
        }

        XMVECTOR cp = XMVectorSubtract(point, c);
        float d5 = VectorDot(ab, cp);
        float d6 = VectorDot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        {
            float w = d2 / (d2 - d6);
            return XMVectorAdd(a, XMVectorScale(ac, w));
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return XMVectorAdd(b, XMVectorScale(XMVectorSubtract(c, b), w));
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return XMVectorAdd(a, XMVectorAdd(XMVectorScale(ab, v), XMVectorScale(ac, w)));
    }

    bool PointInTriangle(FXMVECTOR point, FXMVECTOR a, FXMVECTOR b, FXMVECTOR c)
    {
        XMVECTOR normal = XMVector3Cross(XMVectorSubtract(b, a), XMVectorSubtract(c, a));
        if (VectorLengthSq(normal) <= Epsilon) return false;

        XMVECTOR edge0 = XMVectorSubtract(b, a);
        XMVECTOR edge1 = XMVectorSubtract(c, b);
        XMVECTOR edge2 = XMVectorSubtract(a, c);

        XMVECTOR c0 = XMVector3Cross(edge0, XMVectorSubtract(point, a));
        XMVECTOR c1 = XMVector3Cross(edge1, XMVectorSubtract(point, b));
        XMVECTOR c2 = XMVector3Cross(edge2, XMVectorSubtract(point, c));

        return VectorDot(c0, normal) >= -Epsilon &&
            VectorDot(c1, normal) >= -Epsilon &&
            VectorDot(c2, normal) >= -Epsilon;
    }

    void ClosestPointsBetweenSegmentAndTriangle(FXMVECTOR segmentA, FXMVECTOR segmentB,
        FXMVECTOR triangleA, FXMVECTOR triangleB, FXMVECTOR triangleC,
        XMVECTOR& outSegmentPoint, XMVECTOR& outTrianglePoint)
    {
        float bestDistanceSq = FLT_MAX;

        auto consider = [&](FXMVECTOR segmentPoint, FXMVECTOR trianglePoint)
        {
            float distanceSq = VectorLengthSq(XMVectorSubtract(trianglePoint, segmentPoint));
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                outSegmentPoint = segmentPoint;
                outTrianglePoint = trianglePoint;
            }
        };

        XMVECTOR triangleNormal = XMVector3Cross(XMVectorSubtract(triangleB, triangleA), XMVectorSubtract(triangleC, triangleA));
        XMVECTOR segmentDelta = XMVectorSubtract(segmentB, segmentA);
        float denominator = VectorDot(segmentDelta, triangleNormal);

        if (VectorLengthSq(triangleNormal) > Epsilon && fabsf(denominator) > Epsilon)
        {
            float t = VectorDot(XMVectorSubtract(triangleA, segmentA), triangleNormal) / denominator;
            if (t >= 0.0f && t <= 1.0f)
            {
                XMVECTOR planePoint = XMVectorAdd(segmentA, XMVectorScale(segmentDelta, t));
                if (PointInTriangle(planePoint, triangleA, triangleB, triangleC))
                {
                    outSegmentPoint = planePoint;
                    outTrianglePoint = planePoint;
                    return;
                }
            }
        }

        consider(segmentA, ClosestPointOnTriangle(segmentA, triangleA, triangleB, triangleC));
        consider(segmentB, ClosestPointOnTriangle(segmentB, triangleA, triangleB, triangleC));

        XMVECTOR edgePointA{};
        XMVECTOR edgePointB{};
        ClosestPointsBetweenSegments(segmentA, segmentB, triangleA, triangleB, edgePointA, edgePointB);
        consider(edgePointA, edgePointB);
        ClosestPointsBetweenSegments(segmentA, segmentB, triangleB, triangleC, edgePointA, edgePointB);
        consider(edgePointA, edgePointB);
        ClosestPointsBetweenSegments(segmentA, segmentB, triangleC, triangleA, edgePointA, edgePointB);
        consider(edgePointA, edgePointB);
    }

    bool ComputeCapsuleMeshContact(const CapsuleCollider& capsule, const MeshCollider& mesh, ContactInfo& outContact)
    {
        XMFLOAT3 capsulePointA = capsule.GetPointA();
        XMFLOAT3 capsulePointB = capsule.GetPointB();
        XMFLOAT3 capsuleCenterFloat = capsule.GetCenter();

        XMVECTOR segmentA = XMLoadFloat3(&capsulePointA);
        XMVECTOR segmentB = XMLoadFloat3(&capsulePointB);
        XMVECTOR capsuleCenter = XMLoadFloat3(&capsuleCenterFloat);

        const float radius = capsule.GetRadius();
        const float radiusSq = radius * radius;
        float deepestPenetration = -FLT_MAX;
        XMVECTOR bestNormal = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

        for (const auto& triangle : mesh.GetWorldTriangles())
        {
            XMVECTOR triangleA = XMLoadFloat3(&triangle.a);
            XMVECTOR triangleB = XMLoadFloat3(&triangle.b);
            XMVECTOR triangleC = XMLoadFloat3(&triangle.c);
            XMVECTOR triangleNormal = XMVector3Cross(XMVectorSubtract(triangleB, triangleA), XMVectorSubtract(triangleC, triangleA));

            if (VectorLengthSq(triangleNormal) <= Epsilon) continue;

            XMVECTOR segmentPoint{};
            XMVECTOR trianglePoint{};
            ClosestPointsBetweenSegmentAndTriangle(segmentA, segmentB, triangleA, triangleB, triangleC, segmentPoint, trianglePoint);

            XMVECTOR capsuleToTriangle = XMVectorSubtract(trianglePoint, segmentPoint);
            float distanceSq = VectorLengthSq(capsuleToTriangle);
            if (distanceSq > radiusSq) continue;

            XMVECTOR normal{};
            float penetration = 0.0f;
            if (distanceSq > Epsilon)
            {
                float distance = sqrtf(distanceSq);
                normal = XMVectorScale(capsuleToTriangle, 1.0f / distance);
                penetration = radius - distance;
            }
            else
            {
                triangleNormal = SafeNormalize(triangleNormal, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
                float signedDistance = VectorDot(XMVectorSubtract(capsuleCenter, triangleA), triangleNormal);
                normal = signedDistance >= 0.0f ? XMVectorScale(triangleNormal, -1.0f) : triangleNormal;
                penetration = radius;
            }

            if (penetration > deepestPenetration)
            {
                deepestPenetration = penetration;
                bestNormal = normal;
            }
        }

        if (deepestPenetration < 0.0f) return false;

        outContact.normal = ToFloat3(SafeNormalize(bestNormal, XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f)));
        outContact.penetration = max(deepestPenetration, 0.0f);
        return true;
    }

    bool ComputeCapsuleCapsuleContact(const CapsuleCollider& capsuleA, const CapsuleCollider& capsuleB, ContactInfo& outContact)
    {
        XMFLOAT3 capsuleAPointA = capsuleA.GetPointA();
        XMFLOAT3 capsuleAPointB = capsuleA.GetPointB();
        XMFLOAT3 capsuleBPointA = capsuleB.GetPointA();
        XMFLOAT3 capsuleBPointB = capsuleB.GetPointB();

        XMVECTOR segmentA0 = XMLoadFloat3(&capsuleAPointA);
        XMVECTOR segmentA1 = XMLoadFloat3(&capsuleAPointB);
        XMVECTOR segmentB0 = XMLoadFloat3(&capsuleBPointA);
        XMVECTOR segmentB1 = XMLoadFloat3(&capsuleBPointB);

        XMVECTOR pointA{};
        XMVECTOR pointB{};
        ClosestPointsBetweenSegments(segmentA0, segmentA1, segmentB0, segmentB1, pointA, pointB);

        XMVECTOR delta = XMVectorSubtract(pointB, pointA);
        float distanceSq = VectorLengthSq(delta);
        float radiusSum = capsuleA.GetRadius() + capsuleB.GetRadius();
        float radiusSumSq = radiusSum * radiusSum;

        if (distanceSq > radiusSumSq) return false;

        XMVECTOR normal{};
        float penetration = 0.0f;

        if (distanceSq > Epsilon)
        {
            float distance = sqrtf(distanceSq);
            normal = XMVectorScale(delta, 1.0f / distance);
            penetration = radiusSum - distance;
        }
        else
        {
            XMFLOAT3 centerAFloat = capsuleA.GetCenter();
            XMFLOAT3 centerBFloat = capsuleB.GetCenter();
            XMVECTOR centerA = XMLoadFloat3(&centerAFloat);
            XMVECTOR centerB = XMLoadFloat3(&centerBFloat);

            normal = SafeNormalize(XMVectorSubtract(centerB, centerA), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
            penetration = radiusSum;
        }

        outContact.normal = ToFloat3(normal);
        outContact.penetration = max(penetration, 0.0f);
        return true;
    }

    void ApplyImpulse(const shared_ptr<GameObject>& objA, const shared_ptr<GameObject>& objB, const XMFLOAT3& impulse)
    {
        if (objA && objA->GetInverseMass() > 0.0f) objA->AddImpulse(Utiles::Vector3::Mul(impulse, -1.0f));
        if (objB && objB->GetInverseMass() > 0.0f) objB->AddImpulse(impulse);
    }

    void ResolveVelocity(const shared_ptr<GameObject>& objA, const shared_ptr<GameObject>& objB, const ContactInfo& contact)
    {
        float invMassA = objA->GetInverseMass();
        float invMassB = objB->GetInverseMass();
        float invMassSum = invMassA + invMassB;
        if (invMassSum <= 0.0f) return;

        XMFLOAT3 velA = objA->GetVelocity();
        XMFLOAT3 velB = objB->GetVelocity();
        XMFLOAT3 relativeVel = Utiles::Vector3::Sub(velB, velA);
        float velAlongNormal = Utiles::Vector3::Dot(relativeVel, contact.normal);
        float normalImpulse = 0.0f;

        if (velAlongNormal < 0.0f)
        {
            float restitution = min(objA->GetRestitution(), objB->GetRestitution());
            if (fabsf(velAlongNormal) < RestingVelocity) restitution = 0.0f;

            normalImpulse = -(1.0f + restitution) * velAlongNormal / invMassSum;
            ApplyImpulse(objA, objB, Utiles::Vector3::Mul(contact.normal, normalImpulse));
        }

        if (normalImpulse <= 0.0f) return;

        velA = objA->GetVelocity();
        velB = objB->GetVelocity();
        relativeVel = Utiles::Vector3::Sub(velB, velA);

        float normalSpeed = Utiles::Vector3::Dot(relativeVel, contact.normal);
        XMFLOAT3 tangent = Utiles::Vector3::Sub(relativeVel, Utiles::Vector3::Mul(contact.normal, normalSpeed));
        float tangentLengthSq = Utiles::Vector3::Dot(tangent, tangent);
        if (tangentLengthSq <= Epsilon) return;

        tangent = Utiles::Vector3::Mul(tangent, 1.0f / sqrtf(tangentLengthSq));
        float tangentImpulse = -Utiles::Vector3::Dot(relativeVel, tangent) / invMassSum;
        float maxFriction = normalImpulse * Friction;
        tangentImpulse = clamp(tangentImpulse, -maxFriction, maxFriction);

        ApplyImpulse(objA, objB, Utiles::Vector3::Mul(tangent, tangentImpulse));
    }

    void ApplyPositionCorrection(const shared_ptr<GameObject>& objA, const shared_ptr<GameObject>& objB, const ContactInfo& contact)
    {
        float invMassA = objA->GetInverseMass();
        float invMassB = objB->GetInverseMass();
        float invMassSum = invMassA + invMassB;
        if (invMassSum <= 0.0f) return;

        float correctionAmount = max(contact.penetration - ContactSlop, 0.0f) * PositionCorrectionPercent / invMassSum;
        correctionAmount = min(correctionAmount, MaxPositionCorrectionPerIteration);
        if (correctionAmount <= 0.0f) return;

        XMFLOAT3 correction = Utiles::Vector3::Mul(contact.normal, correctionAmount);
        if (invMassA > 0.0f) objA->Transform(Utiles::Vector3::Mul(correction, -invMassA));
        if (invMassB > 0.0f) objB->Transform(Utiles::Vector3::Mul(correction, invMassB));
    }

    void MarkGrounded(const shared_ptr<GameObject>& object)
    {
        auto rigidbody = object ? object->GetRigidbody() : nullptr;
        if (!rigidbody) return;

        rigidbody->SetGrounded(true);
        XMFLOAT3 velocity = rigidbody->GetVelocity();
        if (velocity.y < 0.0f)
        {
            velocity.y = 0.0f;
            rigidbody->SetVelocity(velocity);
        }
    }

    void UpdateGroundedState(const shared_ptr<GameObject>& objA, const shared_ptr<GameObject>& objB, const ContactInfo& contact)
    {
        if (contact.isTerrainContact && !contact.isWalkable) return;

        if (objA->GetInverseMass() > 0.0f && contact.normal.y < -0.5f) MarkGrounded(objA);
        if (objB->GetInverseMass() > 0.0f && contact.normal.y > 0.5f) MarkGrounded(objB);
    }

    void SlideCharacterOnSteepTerrain(const shared_ptr<GameObject>& objA, const shared_ptr<GameObject>& objB, const ContactInfo& contact)
    {
        if (!contact.isTerrainContact || contact.isWalkable) return;

        shared_ptr<GameObject> character;
        if (IsCharacterObject(objA) && objA->GetInverseMass() > 0.0f) character = objA;
        else if (IsCharacterObject(objB) && objB->GetInverseMass() > 0.0f) character = objB;
        if (!character) return;

        auto rigidbody = character->GetRigidbody();
        if (!rigidbody) return;

        XMFLOAT3 surfaceNormal = Utiles::Vector3::Normalize(contact.surfaceNormal);
        XMFLOAT3 velocity = rigidbody->GetVelocity();
        float intoSurface = Utiles::Vector3::Dot(velocity, surfaceNormal);
        if (intoSurface < 0.0f)
        {
            velocity = Utiles::Vector3::Sub(velocity, Utiles::Vector3::Mul(surfaceNormal, intoSurface));
            rigidbody->SetVelocity(velocity);
        }
    }

    XMFLOAT3 LerpPoint(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return XMFLOAT3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    XMFLOAT3 SubtractPoint(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return XMFLOAT3{ a.x - b.x, a.y - b.y, a.z - b.z };
    }

    struct ContinuousHit
    {
        bool hit = false;
        float time = 1.0f;
        ContactInfo contact{};
    };

    bool TryGetTerrainClearance(const TerrainCollider& terrain, const XMFLOAT3& point,
        float& outClearance, XMFLOAT3& outNormal)
    {
        float height = 0.0f;
        if (!terrain.GetHeightAtWorld(point, height, outNormal)) return false;

        outClearance = point.y - height - ContinuousCollisionSkin;
        return true;
    }

    bool FindTerrainSweepHit(const TerrainCollider& terrain, const XMFLOAT3& start,
        const XMFLOAT3& end, float& outTime, XMFLOAT3& outNormal)
    {
        float startClearance = 0.0f;
        float endClearance = 0.0f;
        XMFLOAT3 startNormal{};
        XMFLOAT3 endNormal{};

        if (!TryGetTerrainClearance(terrain, start, startClearance, startNormal)) return false;
        if (!TryGetTerrainClearance(terrain, end, endClearance, endNormal)) return false;

        if (startClearance <= 0.0f || endClearance > 0.0f) return false;

        float low = 0.0f;
        float high = 1.0f;
        outNormal = endNormal;

        for (int i = 0; i < 12; ++i)
        {
            float mid = (low + high) * 0.5f;
            XMFLOAT3 point = LerpPoint(start, end, mid);
            float clearance = 0.0f;
            XMFLOAT3 normal{};
            if (!TryGetTerrainClearance(terrain, point, clearance, normal)) return false;

            if (clearance > 0.0f)
            {
                low = mid;
            }
            else
            {
                high = mid;
                outNormal = normal;
            }
        }

        outTime = high;
        return true;
    }

    void BuildColliderBottomSamples(const shared_ptr<Collider>& collider, vector<XMFLOAT3>& samples)
    {
        samples.clear();
        if (!collider) return;

        if (collider->GetType() == ColliderType::Box)
        {
            auto box = static_pointer_cast<BoxCollider>(collider);
            XMFLOAT3 points[9]{};
            int count = BuildOBBBottomFaceSamples(box->GetWorldOBB(), points);
            samples.assign(points, points + count);
        }
        else if (collider->GetType() == ColliderType::Mesh)
        {
            auto mesh = static_pointer_cast<MeshCollider>(collider);
            XMFLOAT3 points[9]{};
            int count = BuildOBBBottomFaceSamples(mesh->GetWorldOBB(), points);
            samples.assign(points, points + count);
        }
        else if (collider->GetType() == ColliderType::Capsule)
        {
            auto capsule = static_pointer_cast<CapsuleCollider>(collider);
            XMFLOAT3 pointA = capsule->GetPointA();
            XMFLOAT3 pointB = capsule->GetPointB();
            XMFLOAT3 bottom = pointA.y < pointB.y ? pointA : pointB;
            samples.push_back(XMFLOAT3{ bottom.x, bottom.y - capsule->GetRadius(), bottom.z });
        }
        else if (collider->GetType() == ColliderType::Compound)
        {
            auto compound = static_pointer_cast<CompoundCollider>(collider);
            vector<XMFLOAT3> childSamples;
            for (const auto& child : compound->GetChildren())
            {
                BuildColliderBottomSamples(child.collider, childSamples);
                samples.insert(samples.end(), childSamples.begin(), childSamples.end());
            }
        }
    }

    bool ComputeTerrainContinuousHit(const shared_ptr<Collider>& collider, const TerrainCollider& terrain,
        const XMFLOAT3& displacement, ContinuousHit& nearestHit)
    {
        if (displacement.y >= -ContinuousSweepEpsilon) return false;

        auto owner = collider ? collider->m_owner.lock() : nullptr;
        bool isCharacter = IsCharacterObject(owner);

        vector<XMFLOAT3> samples;
        BuildColliderBottomSamples(collider, samples);
        if (samples.empty()) return false;

        bool hit = false;
        for (const auto& currentSample : samples)
        {
            XMFLOAT3 previousSample = SubtractPoint(currentSample, displacement);
            float hitTime = 1.0f;
            XMFLOAT3 terrainNormal{};
            if (!FindTerrainSweepHit(terrain, previousSample, currentSample, hitTime, terrainNormal)) continue;
            if (hitTime >= nearestHit.time) continue;

            bool walkable = terrainNormal.y >= MaxWalkableSlopeCos;
            if (!walkable) continue;

            XMFLOAT3 pushDirection = GetTerrainPushDirection(terrainNormal, walkable, isCharacter);

            nearestHit.hit = true;
            nearestHit.time = hitTime;
            nearestHit.contact.normal = Utiles::Vector3::Mul(pushDirection, -1.0f);
            nearestHit.contact.surfaceNormal = terrainNormal;
            nearestHit.contact.penetration = ContinuousCollisionSkin;
            nearestHit.contact.isTerrainContact = true;
            nearestHit.contact.isWalkable = walkable;
            nearestHit.contact.slopeAngleDegrees = GetSlopeAngleDegrees(terrainNormal);
            hit = true;
        }

        return hit;
    }

    bool ComputeRaycastContinuousHit(const shared_ptr<Collider>& collider, const shared_ptr<Collider>& staticCollider,
        const XMFLOAT3& displacement, ContinuousHit& nearestHit)
    {
        if (!collider || !staticCollider || staticCollider->GetType() == ColliderType::Terrain) return false;

        XMVECTOR displacementVector = XMLoadFloat3(&displacement);
        float distance = XMVectorGetX(XMVector3Length(displacementVector));
        if (distance <= ContinuousSweepEpsilon) return false;

        XMVECTOR directionVector = XMVectorScale(displacementVector, 1.0f / distance);
        XMFLOAT3 direction = ToFloat3(directionVector);

        vector<XMFLOAT3> samples;
        BuildColliderBottomSamples(collider, samples);
        if (samples.empty()) return false;

        bool hit = false;
        for (const auto& currentSample : samples)
        {
            XMFLOAT3 previousSample = SubtractPoint(currentSample, displacement);
            float hitDistance = 0.0f;
            if (!staticCollider->Raycast(previousSample, direction, hitDistance)) continue;
            if (hitDistance < 0.0f || hitDistance > distance + ContinuousCollisionSkin) continue;

            float hitTime = clamp((hitDistance - ContinuousCollisionSkin) / distance, 0.0f, 1.0f);
            if (hitTime >= nearestHit.time) continue;

            nearestHit.hit = true;
            nearestHit.time = hitTime;
            nearestHit.contact.normal = direction;
            nearestHit.contact.penetration = ContinuousCollisionSkin;
            nearestHit.contact.isTerrainContact = false;
            nearestHit.contact.isWalkable = true;
            hit = true;
        }

        return hit;
    }

    void ClipVelocityAfterContinuousHit(const shared_ptr<GameObject>& object, const ContactInfo& contact)
    {
        auto rigidbody = object ? object->GetRigidbody() : nullptr;
        if (!rigidbody) return;

        XMFLOAT3 pushNormal = Utiles::Vector3::Mul(contact.normal, -1.0f);
        pushNormal = Utiles::Vector3::Normalize(pushNormal);

        XMFLOAT3 velocity = rigidbody->GetVelocity();
        float velocityIntoSurface = Utiles::Vector3::Dot(velocity, pushNormal);
        if (velocityIntoSurface < 0.0f)
        {
            velocity = Utiles::Vector3::Sub(velocity, Utiles::Vector3::Mul(pushNormal, velocityIntoSurface));
            rigidbody->SetVelocity(velocity);
        }

        if ((!contact.isTerrainContact || contact.isWalkable) && pushNormal.y > 0.5f)
        {
            MarkGrounded(object);
        }
    }

    void ApplyContinuousCollisionDetection(const vector<shared_ptr<Collider>>& colliders)
    {
        vector<shared_ptr<Collider>> staticColliders;
        staticColliders.reserve(colliders.size());

        for (const auto& collider : colliders)
        {
            if (!collider) continue;

            auto owner = collider->m_owner.lock();
            if (!IsDynamicObject(owner)) staticColliders.push_back(collider);
        }

        for (const auto& collider : colliders)
        {
            if (!collider || collider->GetType() == ColliderType::Terrain) continue;

            auto owner = collider->m_owner.lock();
            if (!IsDynamicObject(owner)) continue;

            auto rigidbody = owner->GetRigidbody();
            if (!rigidbody || rigidbody->IsKinematic()) continue;
            if (rigidbody->GetCollisionDetectionMode() == CollisionDetectionMode::Discrete) continue;
            if (!rigidbody->HasSweptMotion()) continue;

            XMFLOAT3 displacement = rigidbody->GetLastDisplacement();
            if (Utiles::Vector3::Dot(displacement, displacement) <= ContinuousSweepEpsilon * ContinuousSweepEpsilon) continue;

            ContinuousHit nearestHit{};
            for (const auto& staticCollider : staticColliders)
            {
                if (!staticCollider || staticCollider == collider) continue;

                if (staticCollider->GetType() == ColliderType::Terrain)
                {
                    auto terrain = static_pointer_cast<TerrainCollider>(staticCollider);
                    (void)ComputeTerrainContinuousHit(collider, *terrain, displacement, nearestHit);
                }
                else
                {
                    (void)ComputeRaycastContinuousHit(collider, staticCollider, displacement, nearestHit);
                }
            }

            if (!nearestHit.hit) continue;

            XMFLOAT3 previousPosition = rigidbody->GetPreviousPosition();
            float safeTime = max(nearestHit.time - ContinuousSweepEpsilon, 0.0f);
            XMFLOAT3 correctedPosition{
                previousPosition.x + displacement.x * safeTime,
                previousPosition.y + displacement.y * safeTime,
                previousPosition.z + displacement.z * safeTime
            };

            owner->SetPosition(correctedPosition);
            ClipVelocityAfterContinuousHit(owner, nearestHit.contact);
        }
    }
}

void CollisionManager::AddCollider(const shared_ptr<Collider>& collider)
{
    if (collider) m_colliders.push_back(collider);
}

void CollisionManager::RemoveCollider(const shared_ptr<Collider>& collider)
{
    if (!collider) return;

    erase(m_colliders, collider);
}

void CollisionManager::ClearColliders()
{
    m_colliders.clear();
}

void CollisionManager::Update(bool dispatchEvents)
{
    ApplyContinuousCollisionDetection(m_colliders);

    for (int iteration = 0; iteration < SolverIterations; ++iteration)
    {
        vector<CollisionCandidate> candidates = BuildCollisionCandidates(m_colliders);

        for (const auto& candidate : candidates)
        {
            ContactInfo contact{};
            if (!CheckCollision(candidate.colliderA, candidate.colliderB, contact)) continue;

            ResolveVelocity(candidate.ownerA, candidate.ownerB, contact);
            SlideCharacterOnSteepTerrain(candidate.ownerA, candidate.ownerB, contact);
            ApplyPositionCorrection(candidate.ownerA, candidate.ownerB, contact);
            UpdateGroundedState(candidate.ownerA, candidate.ownerB, contact);

            if (dispatchEvents && iteration == 0) m_eventQueue.push({ candidate.colliderA, candidate.colliderB });
        }
    }

    if (dispatchEvents) ProcessCollisions();
}

void CollisionManager::ProcessCollisions()
{
    while (!m_eventQueue.empty())
    {
        const auto& event = m_eventQueue.front();
        m_eventQueue.pop();

        if (auto ownerA = event.colliderA->m_owner.lock()) ownerA->OnCollisionEnter(event.colliderB);
        if (auto ownerB = event.colliderB->m_owner.lock()) ownerB->OnCollisionEnter(event.colliderA);
    }
}

bool CollisionManager::CheckCollision(const shared_ptr<Collider>& a, const shared_ptr<Collider>& b, ContactInfo& outContact)
{
    if (!a || !b) return false;

    if (a->GetType() == ColliderType::Compound)
    {
        auto compound = static_pointer_cast<CompoundCollider>(a);
        bool foundContact = false;
        float bestPenetration = -FLT_MAX;
        ContactInfo bestContact{};

        for (const auto& child : compound->GetChildren())
        {
            ContactInfo childContact{};
            if (!CheckCollision(child.collider, b, childContact)) continue;
            if (childContact.penetration <= bestPenetration) continue;

            bestPenetration = childContact.penetration;
            bestContact = childContact;
            foundContact = true;
        }

        if (foundContact) outContact = bestContact;
        return foundContact;
    }

    if (b->GetType() == ColliderType::Compound)
    {
        auto compound = static_pointer_cast<CompoundCollider>(b);
        bool foundContact = false;
        float bestPenetration = -FLT_MAX;
        ContactInfo bestContact{};

        for (const auto& child : compound->GetChildren())
        {
            ContactInfo childContact{};
            if (!CheckCollision(a, child.collider, childContact)) continue;
            if (childContact.penetration <= bestPenetration) continue;

            bestPenetration = childContact.penetration;
            bestContact = childContact;
            foundContact = true;
        }

        if (foundContact) outContact = bestContact;
        return foundContact;
    }

    if (a->GetType() == ColliderType::Box && b->GetType() == ColliderType::Box)
    {
        auto boxA = static_pointer_cast<BoxCollider>(a);
        auto boxB = static_pointer_cast<BoxCollider>(b);
        return ComputeOBBOBBContact(boxA->GetWorldOBB(), boxB->GetWorldOBB(), outContact);
    }

    if ((a->GetType() == ColliderType::Box && b->GetType() == ColliderType::Mesh) ||
        (a->GetType() == ColliderType::Mesh && b->GetType() == ColliderType::Box))
    {
        bool isABox = a->GetType() == ColliderType::Box;
        auto box = static_pointer_cast<BoxCollider>(isABox ? a : b);
        auto mesh = static_pointer_cast<MeshCollider>(isABox ? b : a);

        ContactInfo boxContact{};
        if (!ComputeOBBMeshContact(box->GetWorldOBB(), *mesh, boxContact)) return false;

        outContact.normal = isABox ? boxContact.normal : Utiles::Vector3::Mul(boxContact.normal, -1.0f);
        outContact.penetration = boxContact.penetration;
        return true;
    }

    if ((a->GetType() == ColliderType::Box && b->GetType() == ColliderType::Terrain) ||
        (a->GetType() == ColliderType::Terrain && b->GetType() == ColliderType::Box))
    {
        bool isABox = a->GetType() == ColliderType::Box;
        auto box = static_pointer_cast<BoxCollider>(isABox ? a : b);
        auto terrain = static_pointer_cast<TerrainCollider>(isABox ? b : a);

        ContactInfo boxContact{};
        bool limitSteepSlope = IsCharacterObject(box->m_owner.lock());
        if (!ComputeOBBTerrainContact(box->GetWorldOBB(), *terrain, limitSteepSlope, boxContact)) return false;

        outContact.normal = isABox ? boxContact.normal : Utiles::Vector3::Mul(boxContact.normal, -1.0f);
        outContact.penetration = boxContact.penetration;
        return true;
    }

    if (a->GetType() == ColliderType::Mesh && b->GetType() == ColliderType::Mesh)
    {
        auto meshA = static_pointer_cast<MeshCollider>(a);
        auto meshB = static_pointer_cast<MeshCollider>(b);
        return ComputeOBBOBBContact(meshA->GetWorldOBB(), meshB->GetWorldOBB(), outContact);
    }

    if ((a->GetType() == ColliderType::Mesh && b->GetType() == ColliderType::Terrain) ||
        (a->GetType() == ColliderType::Terrain && b->GetType() == ColliderType::Mesh))
    {
        bool isAMesh = a->GetType() == ColliderType::Mesh;
        auto mesh = static_pointer_cast<MeshCollider>(isAMesh ? a : b);
        auto terrain = static_pointer_cast<TerrainCollider>(isAMesh ? b : a);

        ContactInfo meshContact{};
        bool limitSteepSlope = IsCharacterObject(mesh->m_owner.lock());
        if (!ComputeOBBTerrainContact(mesh->GetWorldOBB(), *terrain, limitSteepSlope, meshContact)) return false;

        outContact.normal = isAMesh ? meshContact.normal : Utiles::Vector3::Mul(meshContact.normal, -1.0f);
        outContact.penetration = meshContact.penetration;
        return true;
    }

    bool isACapsule = a->GetType() == ColliderType::Capsule;
    bool isBCapsule = b->GetType() == ColliderType::Capsule;

    if (isACapsule && isBCapsule)
    {
        auto capsuleA = static_pointer_cast<CapsuleCollider>(a);
        auto capsuleB = static_pointer_cast<CapsuleCollider>(b);
        return ComputeCapsuleCapsuleContact(*capsuleA, *capsuleB, outContact);
    }

    if ((isACapsule && b->GetType() == ColliderType::Box) ||
        (a->GetType() == ColliderType::Box && isBCapsule))
    {
        auto capsule = static_pointer_cast<CapsuleCollider>(isACapsule ? a : b);
        auto box = static_pointer_cast<BoxCollider>(isACapsule ? b : a);

        ContactInfo capsuleContact{};
        if (!ComputeCapsuleOBBContact(*capsule, box->GetWorldOBB(), capsuleContact)) return false;

        outContact.normal = isACapsule ? capsuleContact.normal : Utiles::Vector3::Mul(capsuleContact.normal, -1.0f);
        outContact.penetration = capsuleContact.penetration;
        return true;
    }

    if ((isACapsule && b->GetType() == ColliderType::Terrain) ||
        (a->GetType() == ColliderType::Terrain && isBCapsule))
    {
        auto capsule = static_pointer_cast<CapsuleCollider>(isACapsule ? a : b);
        auto terrain = static_pointer_cast<TerrainCollider>(isACapsule ? b : a);

        ContactInfo capsuleContact{};
        bool limitSteepSlope = IsCharacterObject(capsule->m_owner.lock());
        if (!ComputeCapsuleTerrainContact(*capsule, *terrain, limitSteepSlope, capsuleContact)) return false;

        outContact.normal = isACapsule ? capsuleContact.normal : Utiles::Vector3::Mul(capsuleContact.normal, -1.0f);
        outContact.penetration = capsuleContact.penetration;
        return true;
    }

    if ((isACapsule && b->GetType() == ColliderType::Mesh) ||
        (a->GetType() == ColliderType::Mesh && isBCapsule))
    {
        auto capsule = static_pointer_cast<CapsuleCollider>(isACapsule ? a : b);
        auto mesh = static_pointer_cast<MeshCollider>(isACapsule ? b : a);

        ContactInfo capsuleContact{};
        if (!ComputeCapsuleMeshContact(*capsule, *mesh, capsuleContact)) return false;

        outContact.normal = isACapsule ? capsuleContact.normal : Utiles::Vector3::Mul(capsuleContact.normal, -1.0f);
        outContact.penetration = capsuleContact.penetration;
        return true;
    }

    return false;
}

bool CollisionManager::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outHitDist, const shared_ptr<Collider>& ignoreCollider) const
{
    shared_ptr<Collider> hitCollider;
    return Raycast(origin, direction, outHitDist, hitCollider, ignoreCollider);
}

bool CollisionManager::Raycast(const XMFLOAT3& origin, const XMFLOAT3& direction, float& outHitDist,
    shared_ptr<Collider>& outHitCollider, const shared_ptr<Collider>& ignoreCollider) const
{
    bool isHit = false;
    float minHitDist = FLT_MAX;
    outHitCollider.reset();

    for (const auto& collider : m_colliders)
    {
        if (!collider || collider == ignoreCollider) continue;

        float hitDist = 0.0f;
        if (collider->Raycast(origin, direction, hitDist) && hitDist < minHitDist)
        {
            minHitDist = hitDist;
            outHitCollider = collider;
            isHit = true;
        }
    }

    if (isHit) outHitDist = minHitDist;
    return isHit;
}
