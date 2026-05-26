#include "ParticleModuleSizeByLife.h"

#include <algorithm>

#include "Object/Object.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Engine/Particle/Distributions/DistributionVectorCurve.h"

UParticleModuleSizeByLife::UParticleModuleSizeByLife()
{
	auto* DefaultScale = UObjectManager::Get().CreateObject<UDistributionVectorCurve>(this);
	if (DefaultScale)
	{
		DefaultScale->SetConstant(FVector(1.0f, 1.0f, 1.0f));
		LifeMultiplierDistribution = DefaultScale;
	}
}

void UParticleModuleSizeByLife::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
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
		const FVector Scale = LifeMultiplierDistribution
			? LifeMultiplierDistribution->GetValue(T, Owner->GetComponent())
			: FVector(1.0f, 1.0f, 1.0f);

		Particle->Size.X = Particle->BaseSize.X * Scale.X;
		Particle->Size.Y = Particle->BaseSize.Y * Scale.Y;
		Particle->Size.Z = Particle->BaseSize.Z * Scale.Z;
	}
}
