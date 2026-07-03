#pragma once

namespace Utiles
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::exception{};
        }
    }

    namespace Random
    {
        inline INT GetInt(INT min, INT max)
        {
            uniform_int_distribution<INT> dis{ min, max };
            return dis(g_randomEngine);
        }

        inline FLOAT GetFloat(FLOAT min, FLOAT max)
        {
            uniform_real_distribution<FLOAT> dis{ min, max };
            return dis(g_randomEngine);
        }
    }

    namespace Vector3
    {
        inline XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            return XMFLOAT3{ a.x + b.x, a.y + b.y, a.z + b.z };
        }

        inline XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            return XMFLOAT3{ a.x - b.x, a.y - b.y, a.z - b.z };
        }

        inline XMFLOAT3 Mul(const XMFLOAT3& a, FLOAT scalar)
        {
            return XMFLOAT3{ a.x * scalar, a.y * scalar, a.z * scalar };
        }

        inline XMFLOAT3 Negate(const XMFLOAT3& v)
        {
            return XMFLOAT3{ -v.x, -v.y, -v.z };
        }

        inline XMFLOAT3 Normalize(const XMFLOAT3& v)
        {
            XMFLOAT3 result{};
            XMStoreFloat3(&result, XMVector3Normalize(XMLoadFloat3(&v)));
            return result;
        }

        inline XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            XMFLOAT3 result{};
            XMStoreFloat3(&result, XMVector3Cross(XMLoadFloat3(&a), XMLoadFloat3(&b)));
            return result;
        }

        inline XMFLOAT3 Angle(const XMFLOAT3& a, const XMFLOAT3& b, BOOL isNormalized = true)
        {
            XMFLOAT3 result{};
            if (isNormalized) XMStoreFloat3(&result, XMVector3AngleBetweenNormals(XMLoadFloat3(&a), XMLoadFloat3(&b)));
            else XMStoreFloat3(&result, XMVector3AngleBetweenVectors(XMLoadFloat3(&a), XMLoadFloat3(&b)));
            return result;
        }

        inline float Dot(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            return XMVectorGetX(XMVector3Dot(XMLoadFloat3(&a), XMLoadFloat3(&b)));
        }
    }

    namespace Physics
    {
        constexpr float Epsilon = 1.0e-5f;
        constexpr float AxisEpsilon = 1.0e-6f;
        constexpr float ContactSlop = 0.01f;
        constexpr float PositionCorrectionPercent = 0.80f;
        constexpr float MaxPositionCorrectionPerIteration = 0.15f;
        constexpr float ContinuousCollisionSkin = 0.03f;
        constexpr float ContinuousSweepEpsilon = 0.001f;
        constexpr float MaxWalkableSlopeCos = 0.70710678f;
        constexpr int PhysicsSubsteps = 4;
        constexpr int SolverIterations = 5;
        constexpr float Friction = 0.55f;
        constexpr float RestingVelocity = 0.35f;

        inline float VectorLengthSq(FXMVECTOR v)
        {
            return XMVectorGetX(XMVector3LengthSq(v));
        }

        inline float VectorDot(FXMVECTOR a, FXMVECTOR b)
        {
            return XMVectorGetX(XMVector3Dot(a, b));
        }

        inline XMFLOAT3 ToFloat3(FXMVECTOR v)
        {
            XMFLOAT3 result{};
            XMStoreFloat3(&result, v);
            return result;
        }

        inline XMVECTOR SafeNormalize(FXMVECTOR v, FXMVECTOR fallback)
        {
            float lengthSq = VectorLengthSq(v);
            if (lengthSq <= Epsilon) return fallback;
            return XMVectorScale(v, 1.0f / sqrtf(lengthSq));
        }

        inline float GetExtent(const XMFLOAT3& extents, int axis)
        {
            if (axis == 0) return extents.x;
            if (axis == 1) return extents.y;
            return extents.z;
        }

        inline void GetOBBAxes(const BoundingOrientedBox& obb, XMVECTOR axes[3])
        {
            XMVECTOR orientation = XMLoadFloat4(&obb.Orientation);
            axes[0] = SafeNormalize(XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), orientation), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
            axes[1] = SafeNormalize(XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), orientation), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
            axes[2] = SafeNormalize(XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), orientation), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        }

        inline float ProjectOBBRadius(const BoundingOrientedBox& obb, const XMVECTOR axes[3], FXMVECTOR testAxis)
        {
            return obb.Extents.x * fabsf(VectorDot(axes[0], testAxis)) +
                obb.Extents.y * fabsf(VectorDot(axes[1], testAxis)) +
                obb.Extents.z * fabsf(VectorDot(axes[2], testAxis));
        }

        inline bool TestOBBAxis(const BoundingOrientedBox& a, const XMVECTOR axesA[3],
            const BoundingOrientedBox& b, const XMVECTOR axesB[3], FXMVECTOR centerDelta,
            FXMVECTOR axis, float& minOverlap, XMVECTOR& minAxis)
        {
            if (VectorLengthSq(axis) <= AxisEpsilon) return true;

            XMVECTOR normal = XMVector3Normalize(axis);
            float distance = fabsf(VectorDot(centerDelta, normal));
            float overlap = ProjectOBBRadius(a, axesA, normal) + ProjectOBBRadius(b, axesB, normal) - distance;
            if (overlap < 0.0f) return false;

            if (overlap < minOverlap)
            {
                minOverlap = overlap;
                float direction = VectorDot(centerDelta, normal) >= 0.0f ? 1.0f : -1.0f;
                minAxis = XMVectorScale(normal, direction);
            }

            return true;
        }

        inline XMVECTOR ClosestPointOnSegment(FXMVECTOR point, FXMVECTOR a, FXMVECTOR b)
        {
            XMVECTOR ab = XMVectorSubtract(b, a);
            float denom = VectorDot(ab, ab);
            if (denom <= Epsilon) return a;

            float t = VectorDot(XMVectorSubtract(point, a), ab) / denom;
            t = clamp(t, 0.0f, 1.0f);
            return XMVectorAdd(a, XMVectorScale(ab, t));
        }

        inline XMVECTOR ClosestPointOnOBB(FXMVECTOR point, const BoundingOrientedBox& obb, const XMVECTOR axes[3])
        {
            XMVECTOR center = XMLoadFloat3(&obb.Center);
            XMVECTOR delta = XMVectorSubtract(point, center);
            XMVECTOR closest = center;

            for (int i = 0; i < 3; ++i)
            {
                float distance = VectorDot(delta, axes[i]);
                float extent = GetExtent(obb.Extents, i);
                distance = clamp(distance, -extent, extent);
                closest = XMVectorAdd(closest, XMVectorScale(axes[i], distance));
            }

            return closest;
        }

        inline XMVECTOR GetOBBLocalCoordinates(FXMVECTOR point, const BoundingOrientedBox& obb, const XMVECTOR axes[3])
        {
            XMVECTOR center = XMLoadFloat3(&obb.Center);
            XMVECTOR delta = XMVectorSubtract(point, center);
            return XMVectorSet(VectorDot(delta, axes[0]), VectorDot(delta, axes[1]), VectorDot(delta, axes[2]), 0.0f);
        }
    }
}
