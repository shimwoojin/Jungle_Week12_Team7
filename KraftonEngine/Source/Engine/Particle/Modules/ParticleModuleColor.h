#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleColor.generated.h"

// =============================================================================
// UParticleModuleColor
//   Initial Color 전용 모듈.
//   생존 시간에 따른 color/alpha 변화는 UParticleModuleColorOverLife가 담당한다.
// =============================================================================
UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleColor() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Color; }
	const char*     GetDisplayName() const override { return "Initial Color"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;

	UPROPERTY(Edit, Save, Category="Color", DisplayName="Initial Color")
	FVector4 StartColor = { 1, 1, 1, 1 };
};
