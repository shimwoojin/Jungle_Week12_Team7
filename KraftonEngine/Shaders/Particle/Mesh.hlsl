#include "Common/Functions.hlsli"

// Particle Mesh — 정적 mesh + per-instance transform/color로 DrawIndexedInstanced.
// slot 0: 정적 mesh VB (FVertexPNCT layout, 기존 StaticMesh와 동일)
// slot 1: per-instance VB (FParticleMeshInstanceVertex) — INSTANCE_ prefix로 reflection에서 자동 분리

struct VS_Input_MeshParticle
{
    // ---- slot 0 (per-vertex) ----
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
    // ---- slot 1 (per-instance) ----
    // World matrix의 row 0/1/2/3 (translation은 row 3)
    float4 transform0 : INSTANCE_TRANSFORM0;
    float4 transform1 : INSTANCE_TRANSFORM1;
    float4 transform2 : INSTANCE_TRANSFORM2;
    float4 transform3 : INSTANCE_TRANSFORM3;
    float4 instColor  : INSTANCE_COLOR;
    int    subImage   : INSTANCE_SUBIMAGE;
};

struct PS_Input_MeshParticle
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

PS_Input_MeshParticle VS(VS_Input_MeshParticle input)
{
    // row-major world matrix 재구성.
    float4x4 worldMatrix = float4x4(
        input.transform0,
        input.transform1,
        input.transform2,
        input.transform3
    );
    float4 worldPos = mul(float4(input.position, 1.0), worldMatrix);

    PS_Input_MeshParticle output;
    output.position = ApplyVP(worldPos.xyz);
    output.color    = input.color * input.instColor;  // mesh 정점 color × instance tint
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_MeshParticle input) : SV_TARGET
{
    // Day 6 검증: alpha 1.0 강제 — vertex alpha 자산 의존성 제거 + 같은 proxy 내
    // instance 간 정렬 미구현으로 인한 AlphaBlend artifact 회피.
    // 진짜 반투명 필요 시 (1) instance buffer를 CameraDistance로 sort + (2) PS alpha 복원.
    return float4(ApplyWireframe(input.color.rgb), 1.0);
}
