#pragma once

#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/TypeData/ParticleModuleTypeDataBeam.generated.h"

// =============================================================================
// UParticleModuleTypeDataBeam
//   Beam Emitter — Source ↔ Target 두 endpoint 사이에 tessellated quad-strip
//   beam 을 그린다. 각 "입자" 는 beam 의 segment 단위로 보아도 무방.
//   Source/Target 은 외부 (게임/EventGenerator) 가 SetEndpoints 로 갱신.
// =============================================================================
UCLASS()
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataBeam() = default;

	const char* GetDisplayName() const override { return "TypeData (Beam)"; }

	FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent) override;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Interpolation Points", Min=0.0f, Max=128.0f)
	int32 InterpolationPoints = 8;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Noise Amount", Min=0.0f, Max=100.0f)
	float NoiseAmount = 0.0f;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Noise Frequency", Min=0.0f, Max=20.0f)
	float NoiseFrequency = 1.0f;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Tile UV")
	bool bTileUV = true;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Default Source")
	FVector DefaultSource = { 0, 0, 0 };

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Default Target")
	FVector DefaultTarget = { 100, 0, 0 };
};
