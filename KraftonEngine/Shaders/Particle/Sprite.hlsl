#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

// Particle Sprite — CPU에서 이미 월드 좌표로 빌보드 expansion + atlas UV 변환 완료된 정점을 받는다.
// 정점 Color × BaseColor × Opacity × DiffuseTexture로 합성.

// t0: Material.CachedSRVs[Diffuse] (Atlas 텍스처)
Texture2D DiffuseTexture : register(t0);

// b2 (PerShader0): .mat의 Parameters 키에서 자동 매핑
cbuffer ParticleSpriteParams : register(b2)
{
    float4 BaseColor;    // 추가 tint
    float  Opacity;      // [0,1]
    float  UseTexture;   // 0=텍스처 무시(base color만), 1=텍스처 사용
    float2 _pad;
}

struct PS_Input_Sprite
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

PS_Input_Sprite VS(VS_Input_PNCT input)
{
    PS_Input_Sprite output;
    // input.position은 이미 월드 좌표 (CPU에서 빌보드 expansion 완료).
    output.position = ApplyVP(input.position);
    output.color    = input.color;
    // input.texcoord는 이미 atlas tile UV (CPU에서 SubImage 변환 완료) — 그대로.
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Sprite input) : SV_TARGET
{
    float4 sampled = DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    // UseTexture=0이면 texRgb=1, texA=1 — base color만 적용. =1이면 sample 그대로.
    float3 texRgb = lerp(float3(1,1,1), sampled.rgb, UseTexture);
    float  texA   = lerp(1.0,            sampled.a,   UseTexture);

    float3 rgb = input.color.rgb * BaseColor.rgb * texRgb;
    float  a   = input.color.a   * BaseColor.a   * Opacity * texA;
    return float4(ApplyWireframe(rgb), a);
}
