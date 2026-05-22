#include "ParticleEventManager.h"

AParticleEventManager::AParticleEventManager() {}

void AParticleEventManager::HandleParticleSpawnEvents    (UParticleSystemComponent* InPSC, const TArray<FParticleEventSpawnData>&    InEvents) {}
void AParticleEventManager::HandleParticleDeathEvents    (UParticleSystemComponent* InPSC, const TArray<FParticleEventDeathData>&    InEvents) {}
void AParticleEventManager::HandleParticleCollisionEvents(UParticleSystemComponent* InPSC, const TArray<FParticleEventCollideData>&  InEvents) {}
void AParticleEventManager::HandleParticleBurstEvents    (UParticleSystemComponent* InPSC, const TArray<FParticleEventBurstData>&    InEvents) {}
