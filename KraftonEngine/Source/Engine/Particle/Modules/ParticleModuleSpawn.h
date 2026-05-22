#pragma once

#include "Particle/ParticleModule.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSpawn.generated.h"

// =============================================================================
// UParticleModuleSpawn
//   초당 spawn rate 와 1회 burst 를 정의. LOD 당 1개 (Required 옆 슬롯).
//   EmitterInstance::SpawnParticles 가 누적 분수를 가지고 호출.
// =============================================================================
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSpawn() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Spawn; }
	const char*     GetDisplayName() const override { return "Spawn"; }
	bool            IsUnique() const override { return true; }

	// 매 step 호출 — DeltaTime 동안 누적 spawn 분수와 burst count 계산.
	//   OutSpawnAmount   : 정수 + 분수 (예 12.4) — Instance 가 누적 carry.
	//   OutBurstCount    : 이 프레임 1회 burst (없으면 0).
	virtual void GetSpawnAmount(float DeltaTime, float EmitterTime,
	                            float& OutSpawnAmount, int32& OutBurstCount) const;

	// --- Rate ---
	UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Rate (particles/sec)", Min=0.0f, Max=10000.0f)
	float Rate = 20.0f;

	// --- Burst (단순 형태: time → count 단발) ---
	struct FBurstEntry
	{
		float Time   = 0.0f;
		int32 Count  = 0;
		bool  bFired = false; // runtime flag (자동 reset)
	};
	UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Bursts", Type=Array)
	TArray<FBurstEntry> BurstList;

	// --- Scale (LOD 변화 시 rate 스케일) ---
	UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Rate Scale", Min=0.0f, Max=10.0f)
	float RateScale = 1.0f;
};
