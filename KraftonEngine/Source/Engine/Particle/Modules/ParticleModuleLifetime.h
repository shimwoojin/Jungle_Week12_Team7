#pragma once

#include "Particle/ParticleModule.h"

#include "Source/Engine/Particle/Modules/ParticleModuleLifetime.generated.h"

// =============================================================================
// UParticleModuleLifetime
//   입자가 생성될 때 한 번 호출되어 RelativeTime/OneOverMaxLifetime 을 설정.
//   Update 는 EmitterInstance 측이 (RelativeTime += DeltaTime / Lifetime) 처리.
// =============================================================================
UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleLifetime() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Lifetime; }
	const char*     GetDisplayName() const override { return "Lifetime"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;

	// [Min, Max] 균등 샘플 (이후 distribution/curve 로 확장).
	UPROPERTY(Edit, Save, Category="Lifetime", DisplayName="Min Lifetime (sec)", Min=0.0f, Max=60.0f)
	float MinLifetime = 1.0f;

	UPROPERTY(Edit, Save, Category="Lifetime", DisplayName="Max Lifetime (sec)", Min=0.0f, Max=60.0f)
	float MaxLifetime = 2.0f;
};
