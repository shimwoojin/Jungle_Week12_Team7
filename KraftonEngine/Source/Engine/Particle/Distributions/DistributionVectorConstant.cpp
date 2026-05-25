#include "DistributionVectorConstant.h"

void UDistributionVectorConstant::GetRange(FVector& OutMin, FVector& OutMax) const
{
	OutMin = Constant;
	OutMax = Constant;
}
