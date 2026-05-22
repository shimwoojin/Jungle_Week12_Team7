#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Particle/ParticleHelper.h"
#include "Component/Particle/ParticleSystemComponent.h"

class FParticleVertexFactory;
class FParticleDynamicVertexBuffer;

// =============================================================================
// FParticleSystemSceneProxy
//   UParticleSystemComponent 의 렌더 측 mirror.
//   - 매 프레임 PSC 가 만든 FDynamicData (emitter snapshot 묶음) 를 받아 보관
//   - DrawCall 시 각 emitter type 의 VertexFactory 로 dispatch
//   - Dynamic VB 는 proxy 가 소유 (per-frame ring)
//   - Blend: Required.BlendState 에 따라 AlphaBlend/Additive 등 선택
// =============================================================================
class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	~FParticleSystemSceneProxy() override;

	void UpdateTransform()  override;
	void UpdateMaterial()   override;
	void UpdateVisibility() override;
	void UpdateMesh()       override;
	void UpdatePerViewport(const struct FFrameContext& Frame) override;

	// PSC 가 매 프레임 tick 끝에 호출 — proxy 가 ownership 인수.
	void SetDynamicData(UParticleSystemComponent::FDynamicData* InData);

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
	                       FDrawCommandBuffer& OutBuffer) const override;

protected:
	UParticleSystemComponent* GetPSC() const;

	// 매 프레임 교체되는 emitter snapshot. proxy 가 소유.
	UParticleSystemComponent::FDynamicData* DynamicData = nullptr;

	// 각 type 별 vertex factory (proxy 가 소유, lazy init).
	FParticleVertexFactory* SpriteFactory = nullptr;
	FParticleVertexFactory* MeshFactory   = nullptr;
	FParticleVertexFactory* BeamFactory   = nullptr;
	FParticleVertexFactory* RibbonFactory = nullptr;

	// per-proxy dynamic VB (ring).
	FParticleDynamicVertexBuffer* DynamicVB = nullptr;
};
