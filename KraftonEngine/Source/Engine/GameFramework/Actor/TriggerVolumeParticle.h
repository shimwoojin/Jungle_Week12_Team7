#pragma once

#include "GameFramework/Actor/TriggerVolumeBase.h"
#include "Core/Types/CoreTypes.h"
#include "Object/FName.h"

#include "Source/Engine/GameFramework/Actor/TriggerVolumeParticle.generated.h"
class APawn;
class UParticleSystemComponent;

// ============================================================
// ATriggerVolumeParticle — Possessed Pawn 진입/이탈로 파티클 토글
//
// 동작:
//   1) BeginPlay에서 ParticleTag(기본 "particleactor") 를 가진
//      AParticleSystemActor 들을 월드에서 1회 lookup → PSC 캐싱 후 Deactivate
//   2) Pawn 진입(OnPossessedPawnEntered) 시 캐싱된 PSC 들을 Activate
//   3) Pawn 이탈(OnPossessedPawnExited) 시 Deactivate (graceful — 기존 입자는 수명대로 소멸)
//
// 캐싱은 BeginPlay 1회뿐이라 런타임 스폰 파티클은 미반영 (정적 배치 기준).
// 다중 Pawn 동시 진입은 OverlapCount 로 보호.
// ============================================================
UCLASS()
class ATriggerVolumeParticle : public ATriggerVolumeBase
{
public:
	GENERATED_BODY()
	ATriggerVolumeParticle() = default;

	void BeginPlay() override;

	// ATriggerVolumeBase override hooks — 베이스가 Possessed Pawn 필터링 후 호출.
	void OnPossessedPawnEntered(APawn* Pawn) override;
	void OnPossessedPawnExited(APawn* Pawn) override;

private:
	UPROPERTY(Edit, Save, Category="Particle Trigger", DisplayName="ParticleTag")
	FName ParticleTag = "particleactor";  // 직렬화 — 디자이너가 대상 파티클 태그 지정

	UPROPERTY(Edit, Save, Category="Particle Trigger", DisplayName="Activate On Trigger Enter")
	bool bActivateOnTriggerEnter = true;

	UPROPERTY(Edit, Save, Category="Particle Trigger", DisplayName="Deactivate On Trigger Exit")
	bool bDeactivateOnTriggerExit = true;

	TArray<UParticleSystemComponent*> CachedComponents;  // BeginPlay 1회 lookup 캐시
	int32 OverlapCount = 0;                              // 다중 Pawn 동시 진입 보호
};
