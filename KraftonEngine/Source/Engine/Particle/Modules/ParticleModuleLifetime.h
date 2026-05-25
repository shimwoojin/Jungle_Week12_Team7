#pragma once

#include "Particle/ParticleModule.h"
#include "Engine/Particle/Distributions/DistributionFloat.h"

#include "Source/Engine/Particle/Modules/ParticleModuleLifetime.generated.h"

// =============================================================================
// UParticleModuleLifetime
//   입자가 생성될 때 한 번 호출되어 RelativeTime/OneOverMaxLifetime 을 설정.
//   Lifetime 값은 Distribution에서만 평가한다.
// =============================================================================
UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleLifetime();

	EModuleCategory GetCategory() const override { return EModuleCategory::Lifetime; }
	const char*     GetDisplayName() const override { return "Lifetime"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;

	UPROPERTY(Edit, Save, Instanced, Category="Lifetime", DisplayName="Lifetime Distribution", Type=ObjectRef, AllowedClass=UDistributionFloat)
	UDistributionFloat* LifetimeDistribution = nullptr;
};
