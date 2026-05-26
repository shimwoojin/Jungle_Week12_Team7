#include "ParticleModuleSubUV.h"

#include <algorithm>
#include <cmath>

#include "Object/Object.h"
#include "ParticleModuleRequired.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleLODLevel.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Engine/Particle/Distributions/DistributionFloatConstant.h"

namespace
{
	int32 GetSubUVFrameCount(const FParticleEmitterInstance* Owner)
	{
		if (!Owner || !Owner->GetCurrentLOD() || !Owner->GetCurrentLOD()->RequiredModule)
		{
			return 0;
		}

		const UParticleModuleRequired* Required = Owner->GetCurrentLOD()->RequiredModule;
		return Required->SubImagesHorizontal * Required->SubImagesVertical;
	}

	int32 EvaluateSubImageIndex(const UParticleModuleSubUV& Module,
	                          const FParticleEmitterInstance* Owner,
	                          const FBaseParticle& Particle,
	                          int32 FrameCount)
	{
		if (FrameCount <= 0)
		{
			return -1;
		}

		const float EvalTime = std::clamp(Particle.RelativeTime, 0.0f, 1.0f);
		const float IndexValue = Module.SubImageIndexDistribution
			? Module.SubImageIndexDistribution->GetValue(EvalTime, Owner ? Owner->GetComponent() : nullptr)
			: 0.0f;

		int32 FrameIndex = static_cast<int32>(std::floor(IndexValue));
		return std::clamp(FrameIndex, 0, FrameCount - 1);
	}
}

UParticleModuleSubUV::UParticleModuleSubUV()
{
	auto* DefaultIndex = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(this);
	if (DefaultIndex)
	{
		DefaultIndex->Constant = 0.0f;
		SubImageIndexDistribution = DefaultIndex;
	}
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	float SpawnTime, FBaseParticle* Particle)
{
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Particle)
	{
		return;
	}

	const int32 FrameCount = GetSubUVFrameCount(Owner);
	Particle->SubImageIndex = EvaluateSubImageIndex(*this, Owner, *Particle, FrameCount);
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime)
{
	(void)ModuleOffset;
	(void)DeltaTime;

	const int32 FrameCount = GetSubUVFrameCount(Owner);
	if (!Owner || FrameCount <= 0)
	{
		return;
	}

	const uint32 ActiveParticleCount = Owner->GetActiveParticleCount();
	for (uint32 i = 0; i < ActiveParticleCount; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle)
		{
			continue;
		}

		Particle->SubImageIndex = EvaluateSubImageIndex(*this, Owner, *Particle, FrameCount);
	}
}
