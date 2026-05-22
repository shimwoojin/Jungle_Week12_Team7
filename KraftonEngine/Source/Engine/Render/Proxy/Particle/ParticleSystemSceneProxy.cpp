#include "ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Render/Particle/ParticleVertexFactory.h"
#include "Render/Particle/ParticleDynamicVertexBuffer.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// TODO: ProxyFlags 설정 (Translucent — 정렬 필요),
	//       Sprite/Mesh/Beam/Ribbon VertexFactory lazy 생성.
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	delete DynamicData; DynamicData = nullptr;
	delete SpriteFactory; SpriteFactory = nullptr;
	delete MeshFactory;   MeshFactory   = nullptr;
	delete BeamFactory;   BeamFactory   = nullptr;
	delete RibbonFactory; RibbonFactory = nullptr;
	delete DynamicVB;     DynamicVB     = nullptr;
}

void FParticleSystemSceneProxy::UpdateTransform()  {}
void FParticleSystemSceneProxy::UpdateMaterial()   {}
void FParticleSystemSceneProxy::UpdateVisibility() {}
void FParticleSystemSceneProxy::UpdateMesh()       {}
void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame) {}

UParticleSystemComponent* FParticleSystemSceneProxy::GetPSC() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}

void FParticleSystemSceneProxy::SetDynamicData(UParticleSystemComponent::FDynamicData* InData)
{
	delete DynamicData;
	DynamicData = InData;
}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                  FDrawCommandBuffer& OutBuffer) const
{
	// TODO: DynamicData 의 각 emitter 에 대해 type → VertexFactory 디스패치.
	//       BlendState (Required) 에 맞춰 pipeline state 설정.
	//       sort 가 필요한 경우 (ViewDepth) 여기서 처리.
	return false;
}
