#include "light.h"

Light::Light(LightType type) : m_type(type)
{
}

LightShaderData Light::BuildBaseShaderData(float typeValue, const XMFLOAT3& direction) const
{
    const float shaderType = m_enabled ? typeValue : -1.0f;

    return LightShaderData{
        XMFLOAT4{ direction.x, direction.y, direction.z, shaderType },
        XMFLOAT4{ m_position.x, m_position.y, m_position.z, m_range },
        XMFLOAT4{ m_color.x, m_color.y, m_color.z, m_intensity }
    };
}

PointLight::PointLight() : Light(LightType::Point)
{
}

LightShaderData PointLight::BuildShaderData() const
{
    return BuildBaseShaderData(static_cast<float>(LightType::Point), XMFLOAT3{ 0.0f, 0.0f, 1.0f });
}

SpotLight::SpotLight() : Light(LightType::Spot)
{
}

LightShaderData SpotLight::BuildShaderData() const
{
    return BuildBaseShaderData(static_cast<float>(LightType::Spot), m_direction);
}

void SpotLight::SetDirection(const XMFLOAT3& direction)
{
    float lengthSq = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (lengthSq <= Utiles::Physics::Epsilon) return;

    float invLength = 1.0f / sqrtf(lengthSq);
    m_direction = XMFLOAT3{ direction.x * invLength, direction.y * invLength, direction.z * invLength };
}
