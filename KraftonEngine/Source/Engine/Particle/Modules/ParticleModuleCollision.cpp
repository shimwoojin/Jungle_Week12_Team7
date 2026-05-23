#include "ParticleModuleCollision.h"

#include "Particle/ParticleEmitterInstance.h"

#include <algorithm>

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                     float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)SpawnTime;

	if (!Particle)
	{
		return;
	}

	if (FCollisionParticlePayload* Payload =
		PARTICLE_PAYLOAD(Particle, ModuleOffset, FCollisionParticlePayload))
	{
		// collision 카운터는 particle payload에 붙어 있으므로 spawn 시점에 명시적으로 초기화.
		*Payload = {};
	}
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                      float DeltaTime)
{
	(void)DeltaTime;

	if (!Owner)
	{
		return;
	}

	Owner->ForEachActiveParticle([this, ModuleOffset](uint32 ActiveIndex, FBaseParticle& Particle)
		{
			(void)ActiveIndex;

			// particle payload 접근은 module offset 기반의 공식 경로만 사용한다.
			FCollisionParticlePayload* Payload =
				PARTICLE_PAYLOAD(&Particle, ModuleOffset, FCollisionParticlePayload);
			if (!Payload)
			{
				return;
			}

			Payload->NumCollisions = std::max(0, Payload->NumCollisions);

			if (bKillOnCollision && MaxCollisions > 0 && Payload->NumCollisions >= MaxCollisions)
			{
				Particle.Flags |= static_cast<uint32>(EParticleStateFlags::Killed);
			}

			// TODO: 활성 입자의 OldLocation -> Location segment를 실제 LineTrace로 검사하고
			//       hit 시 Velocity 반사, Payload.NumCollisions 증가, CollisionEvent 생성을 연결.
		});
}

uint32 UParticleModuleCollision::RequiredBytes(UParticleLODLevel* LODLevel) const
{
	(void)LODLevel;
	return sizeof(FCollisionParticlePayload);
}
