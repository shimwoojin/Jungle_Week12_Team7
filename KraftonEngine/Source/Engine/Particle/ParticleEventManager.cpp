#include "ParticleEventManager.h"

AParticleEventManager::AParticleEventManager() {}

void AParticleEventManager::HandleParticleSpawnEvents    (UParticleSystemComponent* InPSC, const TArray<FParticleEventSpawnData>&    InEvents)
{
	for (const FParticleEventSpawnData& Event : InEvents)
	{
		OnParticleSpawn.Broadcast(InPSC, Event);
	}
}

void AParticleEventManager::HandleParticleDeathEvents    (UParticleSystemComponent* InPSC, const TArray<FParticleEventDeathData>&    InEvents)
{
	for (const FParticleEventDeathData& Event : InEvents)
	{
		OnParticleDeath.Broadcast(InPSC, Event);
	}
}

void AParticleEventManager::HandleParticleCollisionEvents(UParticleSystemComponent* InPSC, const TArray<FParticleEventCollideData>&  InEvents)
{
	for (const FParticleEventCollideData& Event : InEvents)
	{
		OnParticleCollision.Broadcast(InPSC, Event);
	}
}

void AParticleEventManager::HandleParticleBurstEvents    (UParticleSystemComponent* InPSC, const TArray<FParticleEventBurstData>&    InEvents)
{
	for (const FParticleEventBurstData& Event : InEvents)
	{
		OnParticleBurst.Broadcast(InPSC, Event);
	}
}
