#include "ParticleModuleColor.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                 float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle) return;
	Particle->Color     = StartColor;
	Particle->BaseColor = StartColor;
}

void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                  float DeltaTime)
{
	// TODO: 활성 입자 순회 — Color = lerp(BaseColor → EndColor, RelativeTime).
}
