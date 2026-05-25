#pragma once
#include "Engine/Particle/Distributions/Distribution.h"
#include "Math/Vector.h"
#include "Source/Engine/Particle/Distributions/DistributionVector.generated.h"

UCLASS()
class UDistributionVector : public UDistribution
{
public:
	GENERATED_BODY()

	virtual FVector GetValue(float Time, UObject* Data = nullptr) const;
	virtual void GetRange(FVector& OutMin, FVector& OutMax) const;
};