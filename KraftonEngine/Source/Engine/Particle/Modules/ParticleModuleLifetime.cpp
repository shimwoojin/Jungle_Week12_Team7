#include "ParticleModuleLifetime.h"

#include <algorithm>

#include "Object/Object.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Engine/Particle/Distributions/DistributionFloatUniform.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	auto* DefaultLifetime = UObjectManager::Get().CreateObject<UDistributionFloatUniform>(this);
	if (DefaultLifetime)
	{
		DefaultLifetime->Min = 1.0f;
		DefaultLifetime->Max = 2.0f;
		LifetimeDistribution = DefaultLifetime;
	}
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                    float SpawnTime, FBaseParticle* Particle)
{
	(void)ModuleOffset;

	if (!Particle) return;

	float Life = LifetimeDistribution
		? LifetimeDistribution->GetValue(SpawnTime, Owner ? Owner->GetComponent() : nullptr)
		: 1.0f;

	Life = std::max(0.001f, Life);

	Particle->RelativeTime = std::clamp(SpawnTime, 0.0f, 1.0f);
	Particle->OneOverMaxLifetime = 1.0f / Life;
}
