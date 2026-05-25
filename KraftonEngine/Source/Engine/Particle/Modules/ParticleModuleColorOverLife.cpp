#include "ParticleModuleColorOverLife.h"

#include <algorithm>

#include "Particle/ParticleEmitterInstance.h"

namespace
{
	FVector4 LerpColor(const FVector4& A, const FVector4& B, float T)
	{
		return FVector4(
			A.X + (B.X - A.X) * T,
			A.Y + (B.Y - A.Y) * T,
			A.Z + (B.Z - A.Z) * T,
			A.W + (B.W - A.W) * T);
	}
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
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
		const FVector4 LifeColor = LerpColor(StartColor, EndColor, T);

		if (bMultiplyBaseColor)
		{
			Particle->Color.X = Particle->BaseColor.X * LifeColor.X;
			Particle->Color.Y = Particle->BaseColor.Y * LifeColor.Y;
			Particle->Color.Z = Particle->BaseColor.Z * LifeColor.Z;
			Particle->Color.W = Particle->BaseColor.W * LifeColor.W;
		}
		else
		{
			Particle->Color = LifeColor;
		}
	}
}
