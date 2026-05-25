#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleColorOverLife.generated.h"

// =============================================================================
// UParticleModuleColorOverLife
//   Particle->RelativeTime(0..1)을 기준으로 color/alpha를 보간한다.
//   기본값은 BaseColor를 유지하면서 alpha만 1 -> 0으로 fade out 한다.
// =============================================================================
UCLASS()
class UParticleModuleColorOverLife : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleColorOverLife() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Color; }
	const char*     GetDisplayName() const override { return "Color Over Life"; }

	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Color Over Life", DisplayName="Start Color")
	FVector4 StartColor = { 1, 1, 1, 1 };

	UPROPERTY(Edit, Save, Category="Color Over Life", DisplayName="End Color")
	FVector4 EndColor = { 1, 1, 1, 0 };

	UPROPERTY(Edit, Save, Category="Color Over Life", DisplayName="Multiply Base Color")
	bool bMultiplyBaseColor = true;
};
