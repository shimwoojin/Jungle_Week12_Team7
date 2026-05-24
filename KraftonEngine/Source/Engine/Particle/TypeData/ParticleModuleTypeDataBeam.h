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

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Width", Min=0.0f, Max=10.0f)
	float Width = 0.2f;   // beam 띠 전체 폭 (world units, m) — 입자 스케일(0.2~0.4m)에 맞춤

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Noise Amount", Min=0.0f, Max=100.0f)
	float NoiseAmount = 0.0f;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Noise Frequency", Min=0.0f, Max=20.0f)
	float NoiseFrequency = 1.0f;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Noise Speed", Min=0.0f, Max=20.0f)
	float NoiseSpeed = 2.0f;   // 시간에 따른 noise 흐름 속도 (0 = 정적)

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Tile UV")
	bool bTileUV = true;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Default Source")
	FVector DefaultSource = { 0, 0, 0 };

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Default Target")
	FVector DefaultTarget = { 1, 0, 0 };   // m 단위 — 데모 입자 범위(±1m)와 같은 스케일
};
