#include "ParticleModuleLocation.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle) return;
	// TODO: [Min, Max] 균등 샘플 → bWorldSpaceOverride 따라 local/world 변환.
	Particle->Location    = StartLocationMin;
	Particle->OldLocation = Particle->Location;
}
