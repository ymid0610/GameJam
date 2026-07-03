#pragma once
#include "stdafx.h"

enum class LightType
{
    Point = 0,
    Spot = 1
};

namespace Lighting
{
    constexpr UINT MaxLights = 4;
}

struct LightShaderData
{
    XMFLOAT4 direction;
    XMFLOAT4 position;
    XMFLOAT4 color;
};

struct LightBufferData
{
    XMFLOAT4 meta;
    LightShaderData lights[Lighting::MaxLights];
};

class Light
{
public:
    explicit Light(LightType type);
    virtual ~Light() = default;

    virtual LightShaderData BuildShaderData() const = 0;

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    void SetPosition(const XMFLOAT3& position) { m_position = position; }
    void SetColor(const XMFLOAT3& color) { m_color = color; }
    void SetRange(float range) { m_range = max(0.001f, range); }
    void SetIntensity(float intensity) { m_intensity = max(0.0f, intensity); }

protected:
    LightShaderData BuildBaseShaderData(float typeValue, const XMFLOAT3& direction) const;

protected:
    LightType m_type;
    bool m_enabled = true;
    float m_range = 30.0f;
    float m_intensity = 1.0f;
    XMFLOAT3 m_position{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_color{ 1.0f, 0.95f, 0.82f };
};

class PointLight final : public Light
{
public:
    PointLight();
    LightShaderData BuildShaderData() const override;
};

class SpotLight final : public Light
{
public:
    SpotLight();
    LightShaderData BuildShaderData() const override;

    void SetDirection(const XMFLOAT3& direction);

private:
    XMFLOAT3 m_direction{ 0.0f, 0.0f, 1.0f };
};
