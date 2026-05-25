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

	const EParticleValueSpace SourceSpace =
		bWorldSpaceOverride ? EParticleValueSpace::World : EParticleValueSpace::Local;
	// Spawn location 모듈의 sampled value는 absolute position이 아니라 emitter origin 기준 offset이다.
	const FVector SimulationOffset = Owner->ConvertVectorToSimulation(Offset, SourceSpace);

	Particle->Location = Particle->Location + SimulationOffset;

	Particle->OldLocation = Particle->Location;
}
