#include "Engine/Particle/Distributions/DistributionFloatConstant.h"

void UDistributionFloatConstant::GetOutRange(float& OutMin, float& OutMax) const
{
	OutMin = Constant;
	OutMax = Constant;
}
