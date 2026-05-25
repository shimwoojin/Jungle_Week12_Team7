#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSizeByLife.generated.h"

// =============================================================================
// UParticleModuleSizeByLife
//   Particle->RelativeTime(0..1)을 기준으로 BaseSize에 scale을 곱한다.
//   Curve Distribution이 들어오면 Start/End lerp 대신 RelativeTime curve 평가로 교체 가능하다.
// =============================================================================
UCLASS()
class UParticleModuleSizeByLife : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSizeByLife() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Size; }
	const char*     GetDisplayName() const override { return "Size By Life"; }

	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Size By Life", DisplayName="Start Size Scale")
	FVector StartSizeScale = { 1, 1, 1 };

	UPROPERTY(Edit, Save, Category="Size By Life", DisplayName="End Size Scale")
	FVector EndSizeScale = { 1, 1, 1 };
};
