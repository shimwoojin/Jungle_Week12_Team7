#include "ParticleModuleLifetime.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle) return;
	// TODO: [Min, Max] 균등 샘플 → Particle->OneOverMaxLifetime = 1 / Life
	Particle->RelativeTime        = 0.0f;
	Particle->OneOverMaxLifetime  = 1.0f / MaxLifetime;
}
