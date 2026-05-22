#include "ParticleModuleEventGenerator.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleEventGenerator::HandleSpawnEvent    (FParticleEmitterInstance* Owner, const FParticleEventSpawnData&    InTemplate) const {}
void UParticleModuleEventGenerator::HandleDeathEvent    (FParticleEmitterInstance* Owner, const FParticleEventDeathData&    InTemplate) const {}
void UParticleModuleEventGenerator::HandleCollisionEvent(FParticleEmitterInstance* Owner, const FParticleEventCollideData&  InTemplate) const {}
void UParticleModuleEventGenerator::HandleBurstEvent    (FParticleEmitterInstance* Owner, const FParticleEventBurstData&    InTemplate) const {}
