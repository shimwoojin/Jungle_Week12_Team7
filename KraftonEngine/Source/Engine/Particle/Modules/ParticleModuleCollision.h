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

	// Immediate response answers "what should happen on this hit?"
	enum class ECollisionResponseMode : uint8
	{
		Bounce,
		Stop,
		Kill,
	};

	// Completion mode answers "what should happen once MaxCollisions is reached?"
	enum class ECollisionCompletionMode : uint8
	{
		Kill,
		Freeze,
		IgnoreFurtherCollisions,
	};

	EModuleCategory GetCategory() const override { return EModuleCategory::Collision; }
	const char*     GetDisplayName() const override { return "Collision"; }

	void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	           float SpawnTime, FBaseParticle* Particle) override;
	void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	            float DeltaTime) override;

	uint32 RequiredBytes(UParticleLODLevel* LODLevel) const override;

	// Primarily controls how much of the bounce / normal response is retained
	// after an accepted collision.
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Damping Factor", Min=0.0f, Max=1.0f)
	float DampingFactor = 0.5f;

	// Controls how much surface-parallel velocity is retained after an accepted
	// Bounce collision. Lower values feel more "sticky"; higher values preserve
	// more sliding motion along the hit surface.
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Tangential Damping", Min=0.0f, Max=1.0f)
	float TangentialDamping = 0.75f;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Max Collisions")
	int32 MaxCollisions = 4; // <= 0 means unlimited.

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Channel", Enum=ECollisionChannel)
	ECollisionChannel CollisionChannel = ECollisionChannel::WorldStatic;

	// Immediate response for the current hit. This answers "what happens now?"
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Response Mode", Enum=ECollisionResponseMode)
	ECollisionResponseMode ResponseMode = ECollisionResponseMode::Bounce;

	// Completion behavior once MaxCollisions is reached. This answers
	// "what happens after enough hits?"
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Completion Mode", Enum=ECollisionCompletionMode)
	ECollisionCompletionMode CompletionMode = ECollisionCompletionMode::Freeze;

	// Legacy compatibility path. If true, the immediate response is treated as Kill
	// regardless of ResponseMode so existing assets keep their intent.
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Kill On Collision")
	bool bKillOnCollision = false;

	// Base collision events flow through the existing EventGenerator / PSC path.
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Generate Collision Events")
	bool bGenerateCollisionEvents = false;

	// Per-particle runtime collision state used by the emitter-instance pass.
	// The recent-hit fields are intentionally lightweight and only exist to calm
	// repeated-contact noise; they do not replace response/completion semantics.
	struct FCollisionParticlePayload
	{
		int32 NumCollisions = 0;
		bool bIgnoreFurtherCollisions = false;
		bool bFrozenAfterLimit = false; // Persistently reverts post-update motion at collision pass.
		float LastCollisionTime = -1.0f;
		FVector LastCollisionNormal = FVector::ZeroVector;
	};
};
