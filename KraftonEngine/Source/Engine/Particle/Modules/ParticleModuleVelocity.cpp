#include "ParticleModuleVelocity.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Owner || !Particle) return;

	const float AlphaX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaZ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

	FVector Velocity;
	Velocity.X = StartVelocityMin.X + (StartVelocityMax.X - StartVelocityMin.X) * AlphaX;
	Velocity.Y = StartVelocityMin.Y + (StartVelocityMax.Y - StartVelocityMin.Y) * AlphaY;
	Velocity.Z = StartVelocityMin.Z + (StartVelocityMax.Z - StartVelocityMin.Z) * AlphaZ;

	const EParticleValueSpace SourceSpace =
		bInWorldSpace ? EParticleValueSpace::World : EParticleValueSpace::Local;
	const FVector SimulationVelocity = Owner->ConvertVectorToSimulation(Velocity, SourceSpace);

	Particle->Velocity = SimulationVelocity;
	Particle->BaseVelocity = SimulationVelocity;
}
