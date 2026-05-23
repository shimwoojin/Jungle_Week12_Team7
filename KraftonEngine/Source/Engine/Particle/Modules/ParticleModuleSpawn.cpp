#include "ParticleModuleSpawn.h"

#include "Particle/ParticleEmitterInstance.h"

#include <algorithm>

void UParticleModuleSpawn::GetSpawnAmount(FParticleEmitterInstance* Owner, float DeltaTime, float EmitterTime,
                                          float& OutSpawnAmount, int32& OutBurstCount) const
{
	const float SafeDeltaTime = std::max(0.0f, DeltaTime);
	const float SafeRate = std::max(0.0f, Rate);
	const float SafeRateScale = std::max(0.0f, RateScale);

	OutSpawnAmount = SafeRate * SafeRateScale * SafeDeltaTime;
	OutBurstCount = 0;

	const float CurrentTime = EmitterTime + SafeDeltaTime;
	float PreviousTime = EmitterTime;

	if (Owner)
	{
		// 모듈 자산을 직접 mutate하지 않고 emitter instance payload에
		// "직전 처리 시각"을 저장해 burst trigger를 계산한다.
		if (FSpawnModuleInstancePayload* Payload =
			Owner->GetModuleInstancePayload<FSpawnModuleInstancePayload>(this))
		{
			PreviousTime = Payload->LastProcessedTime;
			if (CurrentTime < PreviousTime)
			{
				PreviousTime = EmitterTime;
			}

			Payload->LastProcessedTime = CurrentTime;
		}
	}

	for (const FBurstEntry& Entry : BurstList)
	{
		if (Entry.Count <= 0)
		{
			continue;
		}

		if (Entry.Time >= PreviousTime && Entry.Time < CurrentTime)
		{
			OutBurstCount += Entry.Count;
		}
	}
}
