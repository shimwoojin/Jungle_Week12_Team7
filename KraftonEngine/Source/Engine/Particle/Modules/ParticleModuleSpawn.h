#pragma once

#include "Particle/ParticleModule.h"
#include "Engine/Particle/Distributions/DistributionFloat.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSpawn.generated.h"

// =============================================================================
// UParticleModuleSpawn
//   초당 spawn rate 와 1회 burst 를 정의. Rate/RateScale은 Distribution에서 평가한다.
// =============================================================================
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSpawn();

	EModuleCategory GetCategory() const override { return EModuleCategory::Spawn; }
	const char*     GetDisplayName() const override { return "Spawn"; }
	bool            IsUnique() const override { return true; }

	struct FSpawnModuleInstancePayload
	{
		float LastProcessedBurstTime = 0.0f;
	};

	virtual void GetRateSpawnAmount(FParticleEmitterInstance* Owner, float DeltaTime, float EmitterTime,
	                                float& OutSpawnAmount) const;
	uint32 RequiredBytesPerInstance() const override { return sizeof(FSpawnModuleInstancePayload); }

	UPROPERTY(Edit, Save, Instanced, Category="Spawn", DisplayName="Rate Distribution", Type=ObjectRef, AllowedClass=UDistributionFloat)
	UDistributionFloat* RateDistribution = nullptr;

	UPROPERTY(Edit, Save, Instanced, Category="Spawn", DisplayName="Rate Scale Distribution", Type=ObjectRef, AllowedClass=UDistributionFloat)
	UDistributionFloat* RateScaleDistribution = nullptr;

	struct FBurstEntry
	{
		float Time   = 0.0f;
		int32 Count  = 0;
	};
	UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Bursts", Type=Array)
	TArray<FBurstEntry> BurstList;
};
