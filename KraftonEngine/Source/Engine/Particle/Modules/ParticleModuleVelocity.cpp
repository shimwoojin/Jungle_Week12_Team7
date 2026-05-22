#include "ParticleModuleVelocity.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle) return;
	// TODO: [Min, Max] 균등 샘플 → bInWorldSpace 따라 변환.
	Particle->Velocity     = StartVelocityMin;
	Particle->BaseVelocity = Particle->Velocity;
}
