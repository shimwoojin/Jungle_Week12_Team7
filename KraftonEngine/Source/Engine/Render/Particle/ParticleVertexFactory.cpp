#include "ParticleVertexFactory.h"

#include "Render/Particle/ParticleDynamicVertexBuffer.h"

// -- Sprite ----
void FParticleSpriteVertexFactory::InitResources(ID3D11Device* Device)    {}
void FParticleSpriteVertexFactory::ReleaseResources()                     {}
bool FParticleSpriteVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                             const FDynamicEmitterReplayDataBase& Replay,
                                             FParticleDynamicVertexBuffer& InOutVB,
                                             FDrawSpec& OutDraw)
{
	// TODO: Replay 의 입자 buffer 를 순회 → FParticleSpriteVertex N 개 작성 → VB 에 업로드.
	return false;
}

// -- Mesh ----
void FParticleMeshVertexFactory::InitResources(ID3D11Device* Device)    {}
void FParticleMeshVertexFactory::ReleaseResources()                     {}
bool FParticleMeshVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                           const FDynamicEmitterReplayDataBase& Replay,
                                           FParticleDynamicVertexBuffer& InOutVB,
                                           FDrawSpec& OutDraw)
{
	// TODO: per-instance transform/color/SubImage 를 instance VB 로 업로드.
	return false;
}

// -- Beam ----
void FParticleBeamVertexFactory::InitResources(ID3D11Device* Device)    {}
void FParticleBeamVertexFactory::ReleaseResources()                     {}
bool FParticleBeamVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                           const FDynamicEmitterReplayDataBase& Replay,
                                           FParticleDynamicVertexBuffer& InOutVB,
                                           FDrawSpec& OutDraw)
{
	// TODO: Source→Target tessellation → quad-strip 정점.
	return false;
}

// -- Ribbon ----
void FParticleRibbonVertexFactory::InitResources(ID3D11Device* Device)    {}
void FParticleRibbonVertexFactory::ReleaseResources()                     {}
bool FParticleRibbonVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                             const FDynamicEmitterReplayDataBase& Replay,
                                             FParticleDynamicVertexBuffer& InOutVB,
                                             FDrawSpec& OutDraw)
{
	// TODO: 시간순 sample 자취 → quad-strip 정점.
	return false;
}
