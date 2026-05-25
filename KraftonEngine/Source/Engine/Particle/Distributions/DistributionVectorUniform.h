#pragma once
#include "Engine/Particle/Distributions/DistributionVector.h"
#include "Source/Engine/Particle/Distributions/DistributionVectorUniform.generated.h"

UCLASS()
class UDistributionVectorUniform : public UDistributionVector
{
public:
	GENERATED_BODY()

	virtual FVector GetValue(float Time, UObject* Data = nullptr) const;
	virtual void GetRange(FVector& OutMin, FVector& OutMax) const;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	FVector Min;

	UPROPERTY(Edit, Save, Category = "Distribution", DisplayName = "Max")
	FVector Max;
};