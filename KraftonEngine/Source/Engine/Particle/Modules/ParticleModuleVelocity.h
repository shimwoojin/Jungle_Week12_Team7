#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleVelocity.generated.h"

// =============================================================================
// UParticleModuleVelocity
//   입자 spawn 시 초기 속도(Velocity, BaseVelocity) 설정.
//   Update 단계에서는 EmitterInstance 가 (Location += Velocity * dt) 처리.
//   추가 가속/감속이 필요하면 Acceleration 모듈을 별도로 둔다 (서브클래스 예정).
// =============================================================================
UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleVelocity() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Velocity; }
	const char*     GetDisplayName() const override { return "Velocity"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;

	UPROPERTY(Edit, Save, Category="Velocity", DisplayName="Start Velocity Min")
	FVector StartVelocityMin = { 0, 0, 0 };

	UPROPERTY(Edit, Save, Category="Velocity", DisplayName="Start Velocity Max")
	FVector StartVelocityMax = { 0, 0, 0 };

	// emitter local 좌표를 world 로 변환할지 (Required.bUseLocalSpace 와 별개).
	UPROPERTY(Edit, Save, Category="Velocity", DisplayName="In World Space")
	bool bInWorldSpace = false;
};
