#include "ParticleModuleColor.h"

#include <algorithm>

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                 float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Particle) return;

	Particle->Color = StartColor;
	Particle->BaseColor = StartColor;
}

void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                  float DeltaTime)
{
	(void)ModuleOffset;
	(void)DeltaTime;

	if (!Owner) return;
	for (uint32 i = 0; i < Owner->GetActiveParticleCount(); ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);

		Particle->Color.X = Particle->BaseColor.X + (EndColor.X - Particle->BaseColor.X) * T;
		Particle->Color.Y = Particle->BaseColor.Y + (EndColor.Y - Particle->BaseColor.Y) * T;
		Particle->Color.Z = Particle->BaseColor.Z + (EndColor.Z - Particle->BaseColor.Z) * T;
		Particle->Color.W = Particle->BaseColor.W + (EndColor.W - Particle->BaseColor.W) * T;
	}
}
