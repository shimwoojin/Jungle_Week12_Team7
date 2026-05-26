#pragma once

#include "Particle/ParticleModule.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleSubUV.generated.h"

// =============================================================================
// UParticleModuleSubUV
//   현재 구현은 StartFrame/EndFrame/FrameRate 기반이다.
//   FrameRate가 0보다 크면 particle age seconds 기준으로 frame을 진행하고,
//   아니면 Particle->RelativeTime(0..1) 기준으로 StartFrame~EndFrame을 보간한다.
//   bLooped가 true면 EndFrame 이후 StartFrame으로 반복하고, false면 EndFrame에 멈춘다.
// =============================================================================
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

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Is Looped")
	bool bLooped = true;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Random Start Frame")
	bool bRandomStartFrame = false;
};
