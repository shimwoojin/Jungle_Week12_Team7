#include "ParticleModuleLocation.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Owner || !Particle) return;

	const float AlphaX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaZ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

	FVector Offset;
	Offset.X = StartLocationMin.X + (StartLocationMax.X - StartLocationMin.X) * AlphaX;
	Offset.Y = StartLocationMin.Y + (StartLocationMax.Y - StartLocationMin.Y) * AlphaY;
	Offset.Z = StartLocationMin.Z + (StartLocationMax.Z - StartLocationMin.Z) * AlphaZ;

	FVector SimulationOffset = Offset;
	if (bWorldSpaceOverride)
	{
		// World-space offset 입력은 현재 emitter의 simulation space(local/world)에 맞춰
		// 변환한 뒤 base spawn location 위에 더한다.
		SimulationOffset = Owner->TransformWorldVectorToSimulation(Offset);
	}
	else
	{
		SimulationOffset = Owner->TransformLocalVectorToSimulation(Offset);
	}

	Particle->Location = Particle->Location + SimulationOffset;

	Particle->OldLocation = Particle->Location;
}
