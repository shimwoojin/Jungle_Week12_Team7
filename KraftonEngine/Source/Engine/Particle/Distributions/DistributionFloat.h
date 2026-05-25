#pragma once
#include "Engine/Particle/Distributions/Distribution.h"

#include "Source/Engine/Particle/Distributions/DistributionFloat.generated.h"

UCLASS()
class UDistributionFloat : public UDistribution
{
public:
	GENERATED_BODY()

	// Time의 의미는 Distribution이 아니라 호출하는 ParticleModule이 결정한다.
	// Initial 모듈은 SpawnTime(emitter loop seconds), Over-Life 모듈은
	// Particle->RelativeTime(0..1)을 넘긴다. Constant/Uniform은 보통 Time을
	// 무시하지만, Curve Distribution은 이 Time으로 실제 값을 평가한다.
	virtual float GetValue(float Time, UObject* Data = nullptr) const;
	virtual void GetOutRange(float& OutMin, float& OutMax) const;
};