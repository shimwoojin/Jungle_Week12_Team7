#include "ParticleModuleSubUV.h"

#include <algorithm>
#include <cmath>
#include "ParticleModuleRequired.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleLODLevel.h"

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime)
{
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

	const uint32 ActiveParticleCount = Owner->GetActiveParticleCount();
	for (uint32 i = 0; i < ActiveParticleCount; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle)
		{
			continue;
		}

		const int32 Frame = std::clamp(static_cast<int32>(floor(Particle->RelativeTime * static_cast<float>(FrameCount))), 
			0, 
			FrameCount - 1);

		Particle->SubImageIndex = Frame;
	}
}
