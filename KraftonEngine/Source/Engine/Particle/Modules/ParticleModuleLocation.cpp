#include "ParticleModuleLocation.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Particle) return;

	const float AlphaX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaZ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

	FVector Offset;
	Offset.X = StartLocationMin.X + (StartLocationMax.X - StartLocationMin.X) * AlphaX;
	Offset.Y = StartLocationMin.Y + (StartLocationMax.Y - StartLocationMin.Y) * AlphaY;
	Offset.Z = StartLocationMin.Z + (StartLocationMax.Z - StartLocationMin.Z) * AlphaZ;

	// TODO: bWorldSpaceOverride는 LocalSpace 정책 정리 후 구현.
	// 현재는 emitter origin 기준 offset만 적용.
	if (bWorldSpaceOverride) Particle->Location = Offset;
	else Particle->Location = Particle->Location + Offset;

	Particle->OldLocation = Particle->Location;
}
