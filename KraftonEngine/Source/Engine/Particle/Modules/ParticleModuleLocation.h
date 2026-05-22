#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleLocation.generated.h"

// =============================================================================
// UParticleModuleLocation
//   입자 spawn 시 초기 위치 결정. 기본은 emitter origin 기준 box 분포.
//   추후 sphere/cylinder/mesh 등으로 서브클래싱 가능 (Location_Sphere 등).
// =============================================================================
UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleLocation() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Location; }
	const char*     GetDisplayName() const override { return "Location"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;

	UPROPERTY(Edit, Save, Category="Location", DisplayName="Start Location Min")
	FVector StartLocationMin = { 0, 0, 0 };

	UPROPERTY(Edit, Save, Category="Location", DisplayName="Start Location Max")
	FVector StartLocationMax = { 0, 0, 0 };

	// emitter local 기준 (false) 또는 world 기준 (true). Required.bUseLocalSpace 와 별개.
	UPROPERTY(Edit, Save, Category="Location", DisplayName="World Space Override")
	bool bWorldSpaceOverride = false;
};
