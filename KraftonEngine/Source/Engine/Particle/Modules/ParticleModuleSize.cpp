#include "ParticleModuleSize.h"

#include <algorithm>

#include "Object/Object.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Engine/Particle/Distributions/DistributionVectorConstant.h"

UParticleModuleSize::UParticleModuleSize()
{
	auto* DefaultStartSize = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(this);
	if (DefaultStartSize)
	{
		DefaultStartSize->Constant = {1, 1, 1};
		StartSizeDistribution = DefaultStartSize;
	}

	auto* DefaultEndScale = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(this);
	if (DefaultEndScale)
	{
		DefaultEndScale->Constant = {1, 1, 1};
		EndSizeScaleDistribution = DefaultEndScale;
	}
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle) return;

	const FVector Size = StartSizeDistribution
		? StartSizeDistribution->GetValue(SpawnTime, Owner ? Owner->GetComponent() : nullptr)
		: FVector(1.0f, 1.0f, 1.0f);

	Particle->Size = Size;
	Particle->BaseSize = Size;

	FSizeParticlePayload* Payload = PARTICLE_PAYLOAD(Particle, ModuleOffset, FSizeParticlePayload);
	if (Payload)
	{
		Payload->EndSizeScale = EndSizeScaleDistribution
			? EndSizeScaleDistribution->GetValue(SpawnTime, Owner ? Owner->GetComponent() : nullptr)
			: FVector(1.0f, 1.0f, 1.0f);
	}
}

void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                 float DeltaTime)
{
	(void)DeltaTime;

	if (!bAnimateOverLife) return;
	if (!Owner) return;

	for (uint32 i = 0; i < Owner->GetActiveParticleCount(); ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle) continue;

		const FSizeParticlePayload* Payload = PARTICLE_PAYLOAD_CONST(Particle, ModuleOffset, FSizeParticlePayload);
		const FVector TargetScale = Payload ? Payload->EndSizeScale : FVector(1.0f, 1.0f, 1.0f);

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);

		const float ScaleX = 1.0f + (TargetScale.X - 1.0f) * T;
		const float ScaleY = 1.0f + (TargetScale.Y - 1.0f) * T;
		const float ScaleZ = 1.0f + (TargetScale.Z - 1.0f) * T;

		Particle->Size.X = Particle->BaseSize.X * ScaleX;
		Particle->Size.Y = Particle->BaseSize.Y * ScaleY;
		Particle->Size.Z = Particle->BaseSize.Z * ScaleZ;
	}
}
