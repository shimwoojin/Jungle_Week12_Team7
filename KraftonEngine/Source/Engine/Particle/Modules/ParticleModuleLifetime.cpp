#include "ParticleModuleLifetime.h"

#include <algorithm>

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)ModuleOffset;

	if (!Particle) return;

	float LifeMin = std::max(0.001f, MinLifetime);
	float LifeMax = std::max(LifeMin, MaxLifetime);

	const float Alpha = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

	const float Life = LifeMin + (LifeMax - LifeMin) * Alpha;

	Particle->RelativeTime = std::clamp(SpawnTime, 0.0f, 1.0f);
	Particle->OneOverMaxLifetime = 1.0f / Life;
}
