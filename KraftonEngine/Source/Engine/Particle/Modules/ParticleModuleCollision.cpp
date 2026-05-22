#include "ParticleModuleCollision.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                      float DeltaTime)
{
	// TODO: 활성 입자 순회 — OldLocation → Location segment 로 LineTrace.
	//       hit 시 Velocity 반사 (DampingFactor), NumCollisions++, bKillOnCollision 처리.
	//       bGenerateCollisionEvents 면 Owner->CollisionEvents 에 push (EventGenerator 가 후처리).
}

uint32 UParticleModuleCollision::RequiredBytes(UParticleLODLevel* LODLevel) const
{
	return sizeof(FCollisionParticlePayload);
}
