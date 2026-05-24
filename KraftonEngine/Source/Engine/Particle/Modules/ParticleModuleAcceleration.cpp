#include "ParticleModuleAcceleration.h"

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleAcceleration::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime)
{
	uint32 ActiveParticles = Owner->GetActiveParticleCount();
	for (uint32 i = 0; i < ActiveParticles; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle) continue;

		Particle->Velocity += Acceleration * DeltaTime;
	}
}
