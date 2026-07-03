static const uint MaxLights = 4;

struct LightData
{
    float4 direction; // xyz: forward, w: -1 off, 0 point, 1 spot
    float4 position;  // xyz: origin, w: range
    float4 color;     // rgb: color, a: intensity
};

cbuffer Lighting : register(b2)
{
    float4 g_lightMeta : packoffset(c0); // x: light count
    LightData g_lights[MaxLights] : packoffset(c1);
};

static const float3 gAmbientColor = float3(0.24f, 0.25f, 0.28f);
static const float gDiffuseStrength = 1.35f;
static const float gSpecularStrength = 0.22f;
static const float gSpecularPower = 48.0f;

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.000001f ? value * rsqrt(lengthSq) : fallback;
}

float SmoothSpotCone(float3 fromLight, float3 lightForward)
{
    const float innerCone = 0.96f;
    const float outerCone = 0.78f;

    float cone = saturate((dot(fromLight, lightForward) - outerCone) / (innerCone - outerCone));
    return cone * cone * (3.0f - 2.0f * cone);
}

float SmoothDistanceAttenuation(float distanceToLight, float lightRange)
{
    float rangeFade = saturate(1.0f - distanceToLight / max(lightRange, 0.001f));
    float physicalFade = 1.0f / (1.0f + distanceToLight * distanceToLight * 0.035f);
    return rangeFade * rangeFade * physicalFade;
}

float3 EvaluateLight(LightData light, float3 albedo, float3 positionW, float3 normal, float3 toCamera)
{
    float3 lightToPixel = positionW - light.position.xyz;
    float distanceToLight = max(length(lightToPixel), 0.001f);
    float3 fromLight = lightToPixel / distanceToLight;
    float3 toLight = -fromLight;
    float3 lightForward = SafeNormalize(light.direction.xyz, float3(0.0f, 0.0f, 1.0f));

    float nDotL = saturate(dot(normal, toLight));
    float lightEnabled = light.direction.w >= 0.0f ? 1.0f : 0.0f;
    float isSpotLight = light.direction.w > 0.5f ? 1.0f : 0.0f;
    float spot = lerp(1.0f, SmoothSpotCone(fromLight, lightForward), isSpotLight);
    float attenuation = SmoothDistanceAttenuation(distanceToLight, light.position.w);
    float intensity = spot * attenuation * lightEnabled * light.color.a;

    float diffuse = nDotL * intensity * gDiffuseStrength;
    float3 reflectedLight = reflect(-toLight, normal);
    float reflectionToCamera = saturate(dot(reflectedLight, toCamera));
    float specular = pow(reflectionToCamera, gSpecularPower) * nDotL * gSpecularStrength * intensity;

    float3 diffuseColor = light.color.rgb * albedo * diffuse;
    float3 specularColor = light.color.rgb * specular;

    return diffuseColor + specularColor;
}

float4 ApplyLight(float3 albedo, float alpha, float3 positionW, float3 normalW, float3 cameraPosition)
{
    float3 normal = SafeNormalize(normalW, float3(0.0f, 1.0f, 0.0f));
    float3 toCamera = SafeNormalize(cameraPosition - positionW, float3(0.0f, 0.0f, -1.0f));
    float3 litColor = gAmbientColor * albedo;

    uint lightCount = min((uint)g_lightMeta.x, MaxLights);
    [unroll]
    for (uint i = 0; i < MaxLights; ++i)
    {
        if (i < lightCount)
        {
            litColor += EvaluateLight(g_lights[i], albedo, positionW, normal, toCamera);
        }
    }

    return float4(saturate(litColor), alpha);
}
