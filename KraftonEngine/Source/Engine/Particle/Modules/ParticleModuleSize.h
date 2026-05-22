#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSize.generated.h"

// =============================================================================
// UParticleModuleSize
//   Spawn 시 BaseSize/Size 설정. Update 에서 size-over-life 적용은 별도 모듈로
//   분리할 수도 있지만 학습용으로 본 클래스에서 함께 처리.
// =============================================================================
UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSize() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Size; }
	const char*     GetDisplayName() const override { return "Size"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Size", DisplayName="Start Size Min")
	FVector StartSizeMin = { 1, 1, 1 };

	UPROPERTY(Edit, Save, Category="Size", DisplayName="Start Size Max")
	FVector StartSizeMax = { 1, 1, 1 };

	UPROPERTY(Edit, Save, Category="Size", DisplayName="End Size Scale")
	FVector EndSizeScale = { 1, 1, 1 };

	UPROPERTY(Edit, Save, Category="Size", DisplayName="Animate Over Life")
	bool bAnimateOverLife = false;
};
