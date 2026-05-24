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
	const char* GetDisplayName() const override { return "SubImage Index"; }

	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime) override;
};