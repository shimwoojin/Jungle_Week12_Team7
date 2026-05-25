#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"
#include "Engine/Particle/Distributions/DistributionVector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSize.generated.h"

// =============================================================================
// UParticleModuleSize
//   Spawn 시 BaseSize/Size를 Distribution으로 설정한다.
//   Update에서 size-over-life도 Distribution으로 샘플된 EndSizeScale을 사용한다.
// =============================================================================
UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSize();

	EModuleCategory GetCategory() const override { return EModuleCategory::Size; }
	const char*     GetDisplayName() const override { return "Size"; }

	struct FSizeParticlePayload
	{
		FVector EndSizeScale = {1, 1, 1};
	};

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;
	uint32 RequiredBytes(UParticleLODLevel* LODLevel) const override { (void)LODLevel; return sizeof(FSizeParticlePayload); }

	UPROPERTY(Edit, Save, Instanced, Category="Size", DisplayName="Start Size Distribution", Type=ObjectRef, AllowedClass=UDistributionVector)
	UDistributionVector* StartSizeDistribution = nullptr;

	UPROPERTY(Edit, Save, Instanced, Category="Size", DisplayName="End Size Scale Distribution", Type=ObjectRef, AllowedClass=UDistributionVector)
	UDistributionVector* EndSizeScaleDistribution = nullptr;

	UPROPERTY(Edit, Save, Category="Size", DisplayName="Animate Over Life")
	bool bAnimateOverLife = false;
};
