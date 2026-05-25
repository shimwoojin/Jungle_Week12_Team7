#include "ParticleModuleSpawn.h"

#include "Object/Object.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Engine/Particle/Distributions/DistributionFloatConstant.h"

#include <algorithm>

UParticleModuleSpawn::UParticleModuleSpawn()
{
	auto* DefaultRate = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(this);
	if (DefaultRate)
	{
		DefaultRate->Constant = 20.0f;
		RateDistribution = DefaultRate;
	}

	auto* DefaultRateScale = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(this);
	if (DefaultRateScale)
	{
		DefaultRateScale->Constant = 1.0f;
		RateScaleDistribution = DefaultRateScale;
	}
}

void UParticleModuleSpawn::GetSpawnAmount(FParticleEmitterInstance* Owner, float DeltaTime, float EmitterTime,
                                          float& OutSpawnAmount, int32& OutBurstCount) const
{
	const float SafeDeltaTime = std::max(0.0f, DeltaTime);
	const float SampledRate = RateDistribution
		? RateDistribution->GetValue(EmitterTime, Owner ? Owner->GetComponent() : nullptr)
		: 0.0f;
	const float SampledRateScale = RateScaleDistribution
		? RateScaleDistribution->GetValue(EmitterTime, Owner ? Owner->GetComponent() : nullptr)
		: 1.0f;

	const float SafeRate = std::max(0.0f, SampledRate);
	const float SafeRateScale = std::max(0.0f, SampledRateScale);

	OutSpawnAmount = SafeRate * SafeRateScale * SafeDeltaTime;
	OutBurstCount = 0;

	const float CurrentTime = EmitterTime + SafeDeltaTime;
	float PreviousTime = EmitterTime;

	if (Owner)
	{
		if (FSpawnModuleInstancePayload* Payload =
			Owner->GetModuleInstancePayload<FSpawnModuleInstancePayload>(this))
		{
			PreviousTime = Payload->LastProcessedTime;
			if (CurrentTime < PreviousTime)
			{
				PreviousTime = EmitterTime;
			}

			Payload->LastProcessedTime = CurrentTime;
		}
	}

	for (const FBurstEntry& Entry : BurstList)
	{
		if (Entry.Count <= 0)
		{
			continue;
		}

		if (Entry.Time >= PreviousTime && Entry.Time < CurrentTime)
		{
			OutBurstCount += Entry.Count;
		}
	}
}
