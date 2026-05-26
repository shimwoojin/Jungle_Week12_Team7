#include "EditorIconPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FEditorIconPass)

FEditorIconPass::FEditorIconPass()
{
	PassType    = ERenderPass::EditorIcon;
	// OverlayFont 와 동일한 오버레이 상태 — 깊이 무시(항상 위) + 알파 블렌드(소프트 엣지).
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
