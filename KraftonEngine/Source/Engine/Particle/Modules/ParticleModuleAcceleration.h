#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"
#include "Engine/Particle/Distributions/DistributionVector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleAcceleration.generated.h"

UCLASS()
class UParticleModuleAcceleration : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleAcceleration();

	EModuleCategory GetCategory() const override { return EModuleCategory::Acceleration; }
	const char* GetDisplayName() const override { return "Const Acceleration"; }

	struct FAccelerationParticlePayload
	{
		FVector Acceleration = {0, 0, 0};
	};

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime) override;
	uint32 RequiredBytes(UParticleLODLevel* LODLevel) const override { (void)LODLevel; return sizeof(FAccelerationParticlePayload); }

	UPROPERTY(Edit, Save, Instanced, Category="Acceleration", DisplayName="Acceleration Distribution", Type=ObjectRef, AllowedClass=UDistributionVector)
	UDistributionVector* AccelerationDistribution = nullptr;
};
