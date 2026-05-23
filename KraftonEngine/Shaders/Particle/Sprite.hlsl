#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"

// Particle Sprite — CPU에서 이미 월드 좌표로 빌보드 expansion된 정점을 받는다.
// 텍스처 없이 정점 Color × BaseColor × Opacity 출력.

// b2 (PerShader0): .mat의 Parameters 키에서 자동 매핑 — BaseColor / Opacity
cbuffer ParticleSpriteParams : register(b2)
{
    float4 BaseColor;   // 추가 tint (.mat에서 조정)
    float  Opacity;     // [0,1] (.mat에서 조정)
    float3 _pad;        // 16-byte 정렬
}

PS_Input_Color VS(VS_Input_PNCT input)
{
    PS_Input_Color output;
    // input.position은 이미 월드 좌표 — Model 곱하지 말고 View*Projection만.
    output.position = ApplyVP(input.position);
    output.color    = input.color;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    float3 rgb = input.color.rgb * BaseColor.rgb;
    float  a   = input.color.a * BaseColor.a * Opacity;
    return float4(ApplyWireframe(rgb), a);
}
