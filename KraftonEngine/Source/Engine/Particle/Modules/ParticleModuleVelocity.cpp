#include "ParticleModuleVelocity.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Particle) return;

	const float AlphaX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaZ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

	FVector Velocity;
	Velocity.X = StartVelocityMin.X + (StartVelocityMax.X - StartVelocityMin.X) * AlphaX;
	Velocity.Y = StartVelocityMin.Y + (StartVelocityMax.Y - StartVelocityMin.Y) * AlphaY;
	Velocity.Z = StartVelocityMin.Z + (StartVelocityMax.Z - StartVelocityMin.Z) * AlphaZ;

	// TODO: bInWorldSpace는 지금은 보류.
	// Required.bUseLocalSpace == true면 local velocity,
	// false면 world velocity로 해석된다.
	// bInWorldSpace 변환은 추후 구현.
	Particle->Velocity = Velocity;
	Particle->BaseVelocity = Velocity;
}
