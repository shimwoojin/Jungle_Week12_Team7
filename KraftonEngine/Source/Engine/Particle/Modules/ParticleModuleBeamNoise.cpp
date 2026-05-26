#include "ParticleModuleBeamNoise.h"

#include "Object/Object.h"
#include "Engine/Particle/Distributions/DistributionFloatConstant.h"

namespace
{
	UDistributionFloatConstant* MakeFloatConstant(UObject* Outer, float Value)
	{
		auto* Distribution = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(Outer);
		if (Distribution)
		{
			Distribution->Constant = Value;
		}
		return Distribution;
	}

	float EvalFloatDistribution(const UDistributionFloat* Distribution, float Time, UObject* Data, float DefaultValue)
	{
		return Distribution ? Distribution->GetValue(Time, Data) : DefaultValue;
	}
}

UParticleModuleBeamNoise::UParticleModuleBeamNoise()
{
	NoiseRangeDistribution = MakeFloatConstant(this, 0.0f);
	FrequencyDistribution = MakeFloatConstant(this, 1.0f);
	NoiseSpeedDistribution = MakeFloatConstant(this, 2.0f);
}

float UParticleModuleBeamNoise::EvaluateNoiseRange(float EmitterTime, UObject* Data) const
{
	const float Value = EvalFloatDistribution(NoiseRangeDistribution, EmitterTime, Data, 0.0f);
	return Value < 0.0f ? 0.0f : Value;
}

float UParticleModuleBeamNoise::EvaluateNoiseFrequency(float EmitterTime, UObject* Data) const
{
	const float Value = EvalFloatDistribution(FrequencyDistribution, EmitterTime, Data, 1.0f);
	return Value < 0.0f ? 0.0f : Value;
}

float UParticleModuleBeamNoise::EvaluateNoiseSpeed(float EmitterTime, UObject* Data) const
{
	return EvalFloatDistribution(NoiseSpeedDistribution, EmitterTime, Data, 2.0f);
}
