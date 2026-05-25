#pragma once
#include "Engine/Particle/Distributions/Distribution.h"

#include "Source/Engine/Particle/Distributions/DistributionFloat.generated.h"

UCLASS()
class UDistributionFloat : public UDistribution
{
public:
	GENERATED_BODY()

	virtual float GetValue(float Time, UObject* Data = nullptr) const;
	virtual void GetOutRange(float& OutMin, float& OutMax) const;
};