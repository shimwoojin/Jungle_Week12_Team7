#include "ParticleModuleSize.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle) return;
	Particle->Size     = StartSizeMin;
	Particle->BaseSize = Particle->Size;
}

void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                 float DeltaTime)
{
	if (!bAnimateOverLife) return;
	// TODO: 활성 입자 순회 — Size = BaseSize * lerp(1 → EndSizeScale, RelativeTime).
}
