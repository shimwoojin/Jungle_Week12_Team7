#include "ParticleModuleSize.h"

#include <algorithm>

#include "Particle/ParticleEmitterInstance.h"

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                float SpawnTime, FBaseParticle* Particle)
{
	(void)Owner;
	(void)ModuleOffset;
	(void)SpawnTime;

	if (!Particle) return;

	const float AlphaX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float AlphaZ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

	FVector Size;
	Size.X = StartSizeMin.X + (StartSizeMax.X - StartSizeMin.X) * AlphaX;
	Size.Y = StartSizeMin.Y + (StartSizeMax.Y - StartSizeMin.Y) * AlphaY;
	Size.Z = StartSizeMin.Z + (StartSizeMax.Z - StartSizeMin.Z) * AlphaZ;

	Particle->Size = Size;
	Particle->BaseSize = Size;
}

void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
                                 float DeltaTime)
{
	(void)ModuleOffset;
	(void)DeltaTime;

	if (!bAnimateOverLife) return;
	if (!Owner) return;

	for (uint32 i = 0; i < Owner->GetActiveParticleCount(); ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);

		const float ScaleX = 1.0f + (EndSizeScale.X - 1.0f) * T;
		const float ScaleY = 1.0f + (EndSizeScale.Y - 1.0f) * T;
		const float ScaleZ = 1.0f + (EndSizeScale.Z - 1.0f) * T;

		Particle->Size.X = Particle->BaseSize.X * ScaleX;
		Particle->Size.Y = Particle->BaseSize.Y * ScaleY;
		Particle->Size.Z = Particle->BaseSize.Z * ScaleZ;
	}
}
