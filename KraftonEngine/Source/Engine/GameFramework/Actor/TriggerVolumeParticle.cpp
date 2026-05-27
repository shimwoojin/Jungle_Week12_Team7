#include "GameFramework/Actor/TriggerVolumeParticle.h"

#include "GameFramework/World.h"
#include "GameFramework/Actor/ParticleSystemActor.h"
#include "GameFramework/GameMode/GameplayStatics.h"
#include "Component/Particle/ParticleSystemComponent.h"

void ATriggerVolumeParticle::BeginPlay()
{
	Super::BeginPlay();  // 베이스의 콜리전 보정 + Begin/End Overlap 델리게이트 바인딩

	// ParticleTag 를 가진 파티클 액터들을 1회 lookup 해 PSC 캐싱. 선형 스캔이라
	// 매 frame 호출은 피하고 여기서만 수행 (FGameplayStatics 주석 권장 패턴).
	CachedComponents.clear();
	for (AActor* Actor : FGameplayStatics::FindActorsByTag(GetWorld(), ParticleTag))
	{
		AParticleSystemActor* ParticleActor = Cast<AParticleSystemActor>(Actor);
		if (!ParticleActor) continue;

		UParticleSystemComponent* PSC = ParticleActor->GetParticleSystemComponent();
		if (!PSC) continue;

		CachedComponents.push_back(PSC);
		PSC->Deactivate();  // 시작 시 꺼둠 — 트리거 진입 전까지 비활성
	}
}

void ATriggerVolumeParticle::OnPossessedPawnEntered(APawn* /*Pawn*/)
{
	// 첫 Pawn 진입에서만 켠다 (이미 켜진 상태에서 추가 진입은 무시).
	if (OverlapCount++ != 0) return;

	for (UParticleSystemComponent* PSC : CachedComponents)
	{
		if (PSC) PSC->Activate();  // bReset=false — 기존 상태 유지하며 spawn 재개
	}
}

void ATriggerVolumeParticle::OnPossessedPawnExited(APawn* /*Pawn*/)
{
	// 마지막 Pawn 이 빠져나갈 때만 끈다.
	if (--OverlapCount > 0) return;
	OverlapCount = 0;  // 음수 방어 (Enter/Exit 비대칭 시)

	for (UParticleSystemComponent* PSC : CachedComponents)
	{
		if (PSC) PSC->Deactivate();  // graceful — 기존 입자는 수명대로 소멸
	}
}
