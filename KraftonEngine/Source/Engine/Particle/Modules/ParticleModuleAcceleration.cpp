#include "ParticleModuleAcceleration.h"

#include "Object/Object.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Engine/Particle/Distributions/DistributionVectorConstant.h"

UParticleModuleAcceleration::UParticleModuleAcceleration()
{
	auto* DefaultAcceleration = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(this);
	if (DefaultAcceleration)
	{
		DefaultAcceleration->Constant = {0, 0, -9.8f};
		AccelerationDistribution = DefaultAcceleration;
	}
}

void UParticleModuleAcceleration::Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float SpawnTime, FBaseParticle* Particle)
{
	if (!Particle)
	{
		return;
	}

	FAccelerationParticlePayload* Payload = PARTICLE_PAYLOAD(Particle, ModuleOffset, FAccelerationParticlePayload);
	if (!Payload)
	{
		return;
	}

	Payload->Acceleration = AccelerationDistribution
		? AccelerationDistribution->GetValue(SpawnTime, Owner ? Owner->GetComponent() : nullptr)
		: FVector(0.0f, 0.0f, 0.0f);
}

void UParticleModuleAcceleration::Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset, float DeltaTime)
{
	if (!Owner)
	{
		return;
	}

	const uint32 ActiveParticleCount = Owner->GetActiveParticleCount();
	for (uint32 i = 0; i < ActiveParticleCount; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAt(i);
		if (!Particle) continue;

		const FAccelerationParticlePayload* Payload = PARTICLE_PAYLOAD_CONST(Particle, ModuleOffset, FAccelerationParticlePayload);
		const FVector SampledAcceleration = Payload ? Payload->Acceleration : FVector(0.0f, 0.0f, 0.0f);
		Particle->Velocity += SampledAcceleration * DeltaTime;
	}
}
