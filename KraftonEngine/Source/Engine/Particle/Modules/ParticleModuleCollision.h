#pragma once

#include "Particle/ParticleModule.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/Modules/ParticleModuleCollision.generated.h"

// =============================================================================
// UParticleModuleCollision
//   매 프레임 활성 입자의 이전 위치 → 현재 위치 segment 를 LineTrace 로 검사.
//   충돌 시:
//     - Velocity 를 반사 (DampingFactor 만큼 감쇠)
//     - EventGenerator 모듈이 등록되어 있으면 CollisionEvent 큐에 push
//   payload (RequiredBytes) 로 충돌 횟수 카운터를 입자별로 보관.
// =============================================================================
UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleCollision() = default;

	EModuleCategory GetCategory() const override { return EModuleCategory::Collision; }
	const char*     GetDisplayName() const override { return "Collision"; }

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

	// EventGenerator 모듈이 등록되어 있을 때만 의미 있음.
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Generate Collision Events")
	bool bGenerateCollisionEvents = false;

	// 입자별 payload 구조 (RequiredBytes 가 sizeof 와 일치).
	struct FCollisionParticlePayload
	{
		int32 NumCollisions = 0;
	};
};
