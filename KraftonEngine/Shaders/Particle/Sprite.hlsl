#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"

// Particle Sprite — CPU에서 이미 월드 좌표로 빌보드 expansion된 정점을 받는다.
// 텍스처 없이 정점 Color를 그대로 출력 (Day 3 단색 먼지 검증용).
// 추후 Day 4에서 Material 텍스처 흘림 + SubUV 추가 예정.

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
    return float4(ApplyWireframe(input.color.rgb), input.color.a);
}
