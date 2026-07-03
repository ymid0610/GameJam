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
    output.color = input.color;

    return output;
}

float4 PIXEL_MAIN(PS_INPUT input) : SV_TARGET
{
    return ApplyLight(input.color.rgb, input.color.a, input.positionW, input.normalW, g_cameraPosition.xyz);
}

float4 PIXEL_UNLIT(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
