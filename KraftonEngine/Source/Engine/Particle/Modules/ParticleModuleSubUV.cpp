#include "ParticleModuleSubUV.h"

#include <algorithm>
#include <cmath>
#include "ParticleModuleRequired.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleLODLevel.h"

#include <cstdlib>

namespace
{
	struct FSubUVFrameRange
	{
		int32 Start = 0;
		int32 End = 0;
		int32 Count = 1;
	};

	FSubUVFrameRange BuildFrameRange(const UParticleModuleSubUV& Module, int32 FrameCount)
	{
		FSubUVFrameRange Range;
		const int32 LastFrame = std::max(0, FrameCount - 1);
		Range.Start = std::clamp(Module.StartFrame, 0, LastFrame);
		Range.End = Module.EndFrame < 0 ? LastFrame : std::clamp(Module.EndFrame, 0, LastFrame);
		if (Range.End < Range.Start)
		{
			Range.End = Range.Start;
		}
		Range.Count = std::max(1, Range.End - Range.Start + 1);
		return Range;
	}

	int32 PickRandomOffset(int32 RangeCount)
	{
		if (RangeCount <= 1)
		{
			return 0;
		}

		return std::rand() % RangeCount;
	}
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)SpawnTime;

	if (!Particle)
	{
		return;
	}

	FSubUVParticlePayload* Payload =
		PARTICLE_PAYLOAD(Particle, ModuleOffset, FSubUVParticlePayload);
	Payload->RandomFrameOffset = 0;

	if (!bRandomStartFrame || !Owner || !Owner->GetCurrentLOD() || !Owner->GetCurrentLOD()->RequiredModule)
	{
		return;
	}

	const UParticleModuleRequired* Required = Owner->GetCurrentLOD()->RequiredModule;
	const int32 FrameCount = Required->SubImagesHorizontal * Required->SubImagesVertical;
	if (FrameCount <= 0)
	{
		return;
	}

	const FSubUVFrameRange Range = BuildFrameRange(*this, FrameCount);
	Payload->RandomFrameOffset = PickRandomOffset(Range.Count);
	Particle->SubImageIndex = Range.Start + Payload->RandomFrameOffset;
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime)
{
	(void)DeltaTime;

	if (!Owner || !Owner->GetCurrentLOD() || !Owner->GetCurrentLOD()->RequiredModule)
	{
		return;
	}

	UParticleModuleRequired* Required = Owner->GetCurrentLOD()->RequiredModule;
	const int32 SubImageHorizontal = Required->SubImagesHorizontal;
	const int32 SubImageVertical = Required->SubImagesVertical;
	const int32 FrameCount = SubImageHorizontal * SubImageVertical;
	if (FrameCount <= 0)
	{
		return;
	}

	const FSubUVFrameRange Range = BuildFrameRange(*this, FrameCount);
	const uint32 ActiveParticleCount = Owner->GetActiveParticleCount();
	for (uint32 i = 0; i < ActiveParticleCount; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle)
		{
			continue;
		}

		const FSubUVParticlePayload* Payload =
			PARTICLE_PAYLOAD_CONST(Particle, ModuleOffset, FSubUVParticlePayload);
		const int32 RandomOffset = bRandomStartFrame ? Payload->RandomFrameOffset : 0;

		int32 SequenceFrame = 0;
		if (FrameRate > 0.0f)
		{
			const float ParticleAge =
				Particle->OneOverMaxLifetime > 0.0f ? Particle->RelativeTime / Particle->OneOverMaxLifetime : 0.0f;
			SequenceFrame = static_cast<int32>(floor(ParticleAge * FrameRate));
		}
		else
		{
			SequenceFrame = static_cast<int32>(floor(Particle->RelativeTime * static_cast<float>(Range.Count)));
		}

		const int32 ClampedSequenceFrame = std::clamp(SequenceFrame, 0, Range.Count - 1);
		const int32 Frame = Range.Start + ((RandomOffset + ClampedSequenceFrame) % Range.Count);

		Particle->SubImageIndex = Frame;
	}
}

uint32 UParticleModuleSubUV::RequiredBytes(UParticleLODLevel* LODLevel) const
{
	(void)LODLevel;
	return sizeof(FSubUVParticlePayload);
}
