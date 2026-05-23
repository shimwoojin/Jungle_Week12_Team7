#include "ParticleModuleSpawn.h"

void UParticleModuleSpawn::GetSpawnAmount(float DeltaTime, float EmitterTime,
                                          float& OutSpawnAmount, int32& OutBurstCount) const
{
	// TODO: Emitter Time을 이용한 fire 갱신 필요
	(void)EmitterTime;

	const float SafeDeltaTime = std::max(0.0f, DeltaTime);
	const float SafeRate = std::max(0.0f, Rate);
	const float SafeRateScale = std::max(0.0f, RateScale);

	OutSpawnAmount = SafeRate * SafeRateScale * SafeDeltaTime;
	OutBurstCount = 0;

	// TODO: BurstList 순회 — 이전 EmitterTime 과 현재 사이 트리거된 entry 의 Count 누적.
	// Burst는 GetSpawnAmount가 const라서 bFired 갱신이 애매함.
	// 나중에 Burst를 제대로 하려면:
	// 1. bFired를 mutable로 바꾸거나
	// 2. Burst runtime state를 EmitterInstance 쪽 payload/state로 빼는 쪽이 좋음.
}

