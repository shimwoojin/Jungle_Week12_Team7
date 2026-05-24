#pragma once
#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleAcceleration.generated.h"

UCLASS()
class UParticleModuleAcceleration : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleAcceleration() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Acceleration; }
	const char* GetDisplayName() const override { return "Const Acceleration"; }

	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Acceleration", DisplayName="Const Acceleration")
	FVector Acceleration = {0, 0, -9.8f};
};