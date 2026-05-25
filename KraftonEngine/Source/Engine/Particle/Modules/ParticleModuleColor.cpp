#include "ParticleModuleColor.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                 float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Particle) return;

	Particle->Color = StartColor;
	Particle->BaseColor = StartColor;
}
