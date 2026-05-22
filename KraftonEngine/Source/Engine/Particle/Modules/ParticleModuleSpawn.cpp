#include "ParticleModuleSpawn.h"

void UParticleModuleSpawn::GetSpawnAmount(float DeltaTime, float EmitterTime,
                                          float& OutSpawnAmount, int32& OutBurstCount) const
{
	OutSpawnAmount = Rate * RateScale * DeltaTime;
	OutBurstCount  = 0;
	// TODO: BurstList 순회 — 이전 EmitterTime 과 현재 사이 트리거된 entry 의 Count 누적.
}
