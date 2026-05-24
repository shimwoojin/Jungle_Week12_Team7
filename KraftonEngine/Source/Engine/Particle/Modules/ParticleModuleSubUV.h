#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSubUV.generated.h"

UCLASS()
class UParticleModuleSubUV : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSubUV() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::SubUV; }
	const char* GetDisplayName() const override { return "SubUV"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime) override;
	uint32 RequiredBytes(UParticleLODLevel* LODLevel) const override;

	struct FSubUVParticlePayload
	{
		int32 RandomFrameOffset = 0;
	};

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Start Frame", Min=0.0f)
	int32 StartFrame = 0;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="End Frame")
	int32 EndFrame = -1;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Frame Rate", Min=0.0f)
	float FrameRate = 0.0f;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Random Start Frame")
	bool bRandomStartFrame = false;
};
