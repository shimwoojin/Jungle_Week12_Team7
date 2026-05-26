#pragma once

#include "Particle/ParticleModule.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleCollision.generated.h"

// =============================================================================
// UParticleModuleCollision
//   Collision authoring/settings module.
//   - Stores collision policy (damping, max collisions, trace channel,
//     kill-on-collision, collision-event intent).
//   - Initializes lightweight per-particle collision payload at spawn time.
//   - Actual world hit query and runtime collision response are executed by the
//     explicit FParticleEmitterInstance collision pass.
// =============================================================================
UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleCollision() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Collision; }
	const char*     GetDisplayName() const override { return "Collision"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	uint32 RequiredBytes(UParticleLODLevel* LODLevel) const override;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Damping Factor", Min=0.0f, Max=1.0f)
	float DampingFactor = 0.5f;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Max Collisions")
	int32 MaxCollisions = 4;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Channel", Enum=ECollisionChannel)
	ECollisionChannel CollisionChannel = ECollisionChannel::WorldStatic;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Kill On Collision")
	bool bKillOnCollision = false;

	// Base collision events flow through the existing EventGenerator / PSC path.
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Generate Collision Events")
	bool bGenerateCollisionEvents = false;

	// Per-particle runtime collision state used by the emitter-instance pass.
	struct FCollisionParticlePayload
	{
		int32 NumCollisions = 0;
	};
};
