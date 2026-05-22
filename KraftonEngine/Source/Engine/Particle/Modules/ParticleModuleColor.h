#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleColor.generated.h"

// =============================================================================
// UParticleModuleColor
//   Spawn 시 초기 color, Update 시 lifetime 에 따른 color 변화 (선형).
//   추후 FloatCurve / FColorCurve 로 대체 가능 (현재는 단순 lerp).
// =============================================================================
UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleColor() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Color; }
	const char*     GetDisplayName() const override { return "Color"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Color", DisplayName="Start Color")
	FVector4 StartColor = { 1, 1, 1, 1 };

	UPROPERTY(Edit, Save, Category="Color", DisplayName="End Color")
	FVector4 EndColor   = { 1, 1, 1, 0 };
};
