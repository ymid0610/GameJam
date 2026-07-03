cbuffer GameObject : register(b0)
{
    matrix g_worldMatrix : packoffset(c0);
};

cbuffer Camera : register(b1)
{
    matrix g_viewMatrix : packoffset(c0);
    matrix g_projectionMatrix : packoffset(c4);
    float4 g_cameraPosition : packoffset(c8);
};

cbuffer Material : register(b3)
{
    float4 g_materialBaseColor : packoffset(c0);
    float4 g_materialEmission : packoffset(c1);
    float4 g_materialSurface : packoffset(c2);
};

cbuffer Shadow : register(b4)
{
    matrix g_shadowViewProjection : packoffset(c0);
    float4 g_shadowData : packoffset(c4); // x: enabled, y: bias, z: darkest factor, w: texel size
};

Texture2D g_shadowMap : register(t0);
SamplerComparisonState g_shadowSampler : register(s0);

#include "light.hlsl"

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float4 shadowPosition : TEXCOORD0;
    float4 color : COLOR;
};

PS_INPUT VERTEX_MAIN(VS_INPUT input)
{
    PS_INPUT output;
    float4 positionW = mul(float4(input.position, 1.0f), g_worldMatrix);

    output.position = mul(positionW, g_viewMatrix);
    output.position = mul(output.position, g_projectionMatrix);
    output.positionW = positionW.xyz;
    output.normalW = normalize(mul(input.normal, (float3x3)g_worldMatrix));
    output.shadowPosition = float4(0.0f, 0.0f, 0.0f, 1.0f);
    output.color = input.color;

    return output;
}

PS_INPUT VERTEX_SHADOW_MAIN(VS_INPUT input)
{
    PS_INPUT output;
    float4 positionW = mul(float4(input.position, 1.0f), g_worldMatrix);

    output.position = mul(positionW, g_viewMatrix);
    output.position = mul(output.position, g_projectionMatrix);
    output.positionW = positionW.xyz;
    output.normalW = normalize(mul(input.normal, (float3x3)g_worldMatrix));
    output.shadowPosition = mul(positionW, g_shadowViewProjection);
    output.color = input.color;

    return output;
}

float Hash21(float2 p)
{
    p = frac(p * float2(127.1f, 311.7f));
    p += dot(p, p + 19.19f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float FractalNoise(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        value += ValueNoise(p) * amplitude;
        p = p * 2.03f + 17.31f;
        amplitude *= 0.5f;
    }
    return value;
}

float3 TerrainProceduralAlbedo(PS_INPUT input)
{
    float3 normal = normalize(input.normalW);
    float tiling = max(g_materialSurface.w, 0.04f);
    float2 uv = input.positionW.xz * tiling;

    float broadNoise = FractalNoise(uv * 1.2f);
    float fineNoise = FractalNoise(uv * 7.0f);
    float pebbleNoise = FractalNoise(uv * 18.0f);

    float slope = saturate((1.0f - normal.y) * 1.65f);
    float highGround = saturate((input.positionW.y - 1.2f) * 0.18f);
    float lowDirt = saturate((-input.positionW.y - 0.15f) * 0.35f);
    float rockMask = saturate(slope * 1.15f + highGround * 0.55f + (pebbleNoise - 0.55f) * 0.35f);
    float dirtMask = saturate(lowDirt + (1.0f - normal.y) * 0.45f + (broadNoise - 0.48f) * 0.25f);

    float3 grass = lerp(float3(0.10f, 0.28f, 0.09f), float3(0.24f, 0.47f, 0.16f), broadNoise);
    grass += (fineNoise - 0.5f) * float3(0.06f, 0.10f, 0.04f);

    float3 dirt = lerp(float3(0.29f, 0.22f, 0.14f), float3(0.48f, 0.38f, 0.23f), fineNoise);
    float3 rock = lerp(float3(0.29f, 0.31f, 0.30f), float3(0.62f, 0.60f, 0.55f), pebbleNoise);
    rock *= lerp(0.82f, 1.18f, FractalNoise(uv * 32.0f));
    float3 roadStone = lerp(float3(0.34f, 0.32f, 0.27f), float3(0.58f, 0.55f, 0.48f), pebbleNoise);
    roadStone = lerp(roadStone, float3(0.24f, 0.20f, 0.15f), saturate((fineNoise - 0.52f) * 1.8f));

    float3 albedo = lerp(grass, dirt, dirtMask * 0.82f);
    albedo = lerp(albedo, rock, rockMask);
    albedo = lerp(albedo, roadStone, saturate(input.color.a));
    albedo *= lerp(0.90f, 1.08f, input.color.g);
    return saturate(albedo * g_materialBaseColor.rgb);
}

float4 PIXEL_MAIN(PS_INPUT input) : SV_TARGET
{
    float useTerrainTexture = step(0.5f, g_materialSurface.z);
    float3 terrainColor = TerrainProceduralAlbedo(input);
    float4 baseColor = float4(lerp(g_materialBaseColor.rgb, terrainColor, useTerrainTexture), g_materialBaseColor.a);
    float4 litColor = ApplyLight(baseColor.rgb, baseColor.a, input.positionW, input.normalW, g_cameraPosition.xyz);
    litColor.rgb = saturate(litColor.rgb + g_materialEmission.rgb * g_materialEmission.a);
    return litColor;
}

float SampleShadowVisibility(float4 shadowPosition)
{
    float3 projected = shadowPosition.xyz / max(shadowPosition.w, 0.0001f);
    float2 uv = projected.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float enabled = step(0.5f, g_shadowData.x);
    float inside = step(0.0f, uv.x) * step(uv.x, 1.0f) *
        step(0.0f, uv.y) * step(uv.y, 1.0f) *
        step(0.0f, projected.z) * step(projected.z, 1.0f);

    float depth = projected.z - g_shadowData.y;
    float texel = max(g_shadowData.w, 0.0001f);
    float visibility = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            visibility += g_shadowMap.SampleCmpLevelZero(
                g_shadowSampler,
                uv + float2(x, y) * texel,
                depth);
        }
    }

    visibility /= 9.0f;
    return lerp(1.0f, visibility, enabled * inside);
}

float4 PIXEL_SHADOW_MAIN(PS_INPUT input) : SV_TARGET
{
    float useTerrainTexture = step(0.5f, g_materialSurface.z);
    float3 terrainColor = TerrainProceduralAlbedo(input);
    float4 baseColor = float4(lerp(g_materialBaseColor.rgb, terrainColor, useTerrainTexture), g_materialBaseColor.a);
    float4 litColor = ApplyLight(baseColor.rgb, baseColor.a, input.positionW, input.normalW, g_cameraPosition.xyz);
    float shadowVisibility = SampleShadowVisibility(input.shadowPosition);
    float shadowFactor = lerp(g_shadowData.z, 1.0f, shadowVisibility);
    litColor.rgb *= shadowFactor;
    litColor.rgb = saturate(litColor.rgb + g_materialEmission.rgb * g_materialEmission.a);
    return litColor;
}

float4 PIXEL_UNLIT(PS_INPUT input) : SV_TARGET
{
    float4 baseColor = g_materialBaseColor;
    baseColor.rgb = saturate(baseColor.rgb + g_materialEmission.rgb * g_materialEmission.a);
    return baseColor;
}

float4 PIXEL_VERTEX_COLOR_UNLIT(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
