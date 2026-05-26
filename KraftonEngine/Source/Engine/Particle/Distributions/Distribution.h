#pragma once
#include "Object/Object.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Source/Engine/Particle/Distributions/Distribution.generated.h"

UCLASS()
class UDistribution : public UObject
{
public:
	GENERATED_BODY()

	virtual void GetInRange(float& OutMin, float& OutMax) const;
};