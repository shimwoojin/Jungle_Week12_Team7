#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"
#include "Engine/Particle/Distributions/DistributionFloat.h"
#include "Engine/Particle/Distributions/DistributionVector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleColorOverLife.generated.h"

// =============================================================================
// UParticleModuleColorOverLife
//   Particle->RelativeTime(0..1)을 기준으로 color/alpha distribution을 평가한다.
//   Unreal Cascade의 Color Over Life처럼 Color는 vector distribution, Alpha는 float
//   distribution으로 분리한다.
// =============================================================================
UCLASS()
class UParticleModuleColorOverLife : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleColorOverLife();

	EModuleCategory GetCategory() const override { return EModuleCategory::Color; }
	const char*     GetDisplayName() const override { return "Color Over Life"; }

	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	UPROPERTY(Edit, Save, Instanced, Category="Color Over Life", DisplayName="Color Distribution", Type=ObjectRef, AllowedClass=UDistributionVector)
	UDistributionVector* ColorOverLifeDistribution = nullptr;

	UPROPERTY(Edit, Save, Instanced, Category="Color Over Life", DisplayName="Alpha Distribution", Type=ObjectRef, AllowedClass=UDistributionFloat)
	UDistributionFloat* AlphaOverLifeDistribution = nullptr;

	UPROPERTY(Edit, Save, Category="Color Over Life", DisplayName="Multiply Base Color")
	bool bMultiplyBaseColor = true;
};
