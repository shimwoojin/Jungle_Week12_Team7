#include "ParticleModuleSizeByLife.h"

#include <algorithm>

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleSizeByLife::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                       float DeltaTime)
{
	(void)ModuleOffset;
	(void)DeltaTime;

	if (!Owner) return;

	for (uint32 i = 0; i < Owner->GetActiveParticleCount(); ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		const FVector Scale = StartSizeScale + (EndSizeScale - StartSizeScale) * T;

		Particle->Size.X = Particle->BaseSize.X * Scale.X;
		Particle->Size.Y = Particle->BaseSize.Y * Scale.Y;
		Particle->Size.Z = Particle->BaseSize.Z * Scale.Z;
	}
}
