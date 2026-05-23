#pragma once

#include "Particle/ParticleHelper.h"
#include "Particle/ParticleEvents.h"
#include "Math/Transform.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;
class UParticleModule;
struct FBaseParticle;
struct FDynamicEmitterDataBase;

// =============================================================================
// FParticleEmitterInstance
//   런타임 emitter 인스턴스. 한 PSC 의 emitter 1개당 하나.
//   - 입자 buffer (ParticleData/ParticleIndices) 관리
//   - 모듈 Spawn/Update 호출
//   - GameThread tick 의 끝에서 GetDynamicData() 로 RenderThread 용 snapshot 생성
//
//   UObject 아님 (raw struct/class). PSC 가 소유 (unique_ptr 동등).
// =============================================================================
class FParticleEmitterInstance
{
public:
	FParticleEmitterInstance() = default;
	virtual ~FParticleEmitterInstance();

	// PSC 가 emitter 에셋 + 자신을 묶어 초기화. 모듈 layout 캐시도 이 시점에 적용.
	virtual void Init(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent);

	// 시뮬레이션 1 step. Spawn → Update → Cull 흐름.
	virtual void Tick(float DeltaTime);

	// 강제 리셋 (입자 전부 제거, accumulated time 초기화).
	virtual void Reset();

	// 현재 활성 입자 수 → snapshot 으로 옮겨 RenderThread 로 전달.
	// 호출 측이 unique_ptr 처럼 소유권을 받는다 (PSC 가 매 프레임 delete).
	virtual FDynamicEmitterDataBase* GetDynamicData();

	// 모듈 → payload offset 조회 헬퍼.
	uint32 GetModuleDataOffset(const UParticleModule* InModule) const;

	// 활성 입자 i 의 BaseParticle 포인터.
	FBaseParticle*       GetParticleAt(uint32 InActiveIndex);
	const FBaseParticle* GetParticleAt(uint32 InActiveIndex) const;

	uint32 GetActiveParticleCount() const { return ActiveParticles; }
	uint32 GetMaxParticleCount()    const { return MaxActiveParticles; }

	// EventManager 가 디스패치할 큐. EventGenerator 모듈이 push.
	const TArray<FParticleEventSpawnData>&     GetSpawnEvents()     const { return SpawnEvents; }
	const TArray<FParticleEventDeathData>&     GetDeathEvents()     const { return DeathEvents; }
	const TArray<FParticleEventCollideData>&   GetCollisionEvents() const { return CollisionEvents; }
	const TArray<FParticleEventBurstData>&     GetBurstEvents()     const { return BurstEvents; }

	void ClearPendingEvents();

	UParticleEmitter*           GetEmitter()       const { return Emitter; }
	UParticleSystemComponent*   GetComponent()     const { return Component; }
	UParticleLODLevel*          GetCurrentLOD()    const;

	// World ↔ Local 헬퍼 (Required.bUseLocalSpace 따라 분기).
	FTransform GetComponentToWorld() const;

protected:
	// --- 내부 도우미 (subclass 가 override 가능) ----------------------------------
	// SpawnFraction 누적 → 정수 입자 수로 변환하여 SpawnInternal 호출.
	virtual void SpawnParticles(float DeltaTime);

	// 새 입자 N 개를 만든다. 내부적으로 모듈의 Spawn() 을 차례로 호출.
	virtual void SpawnInternal(int32 Count, float SpawnTimeBase);

	// 활성 입자 전체에 대해 모듈 Update + RelativeTime 적용 + Kill.
	virtual void UpdateParticles(float DeltaTime);

	// MaxActiveParticles 변경 시 ParticleData/Indices 를 재할당.
	void ResizeParticleData(uint32 NewMax);

	void FillReplayData(FDynamicEmitterReplayDataBase& OutData) const;

private:
	uint32 GetInitialParticleCapacity() const;
	uint32 GrowParticleCapacity(uint32 Current, uint32 Required) const;
	bool IsParticleKilled(const FBaseParticle* Particle) const;
	void ClearSpawnedFlag(FBaseParticle* Particle) const;

protected:
	UParticleEmitter*         Emitter     = nullptr;
	UParticleSystemComponent* Component   = nullptr;

	// 모듈 payload offset 캐시 — emitter 가 미리 계산해 둔 것을 가져와 빠른 조회.
	const TMap<const UParticleModule*, uint32>* ModuleOffsetMap = nullptr;

	// 입자 buffer (BaseParticle + payload) flat layout.
	uint32           ParticleStride       = sizeof(FBaseParticle);
	uint32           MaxActiveParticles   = 0;
	uint32           ActiveParticles      = 0;
	FParticleStorage RuntimeStorage;

	// Spawn 누적 (분수 입자 carry-over)
	float SpawnFraction      = 0.0f;
	float EmitterTimeSeconds = 0.0f; // 자신의 sim time
	int32 LoopCount          = 0;
	int32 CurrentLODIndex    = 0;

	// 이벤트 큐 (EventGenerator 모듈이 push, PSC/EventManager 가 drain).
	TArray<FParticleEventSpawnData>     SpawnEvents;
	TArray<FParticleEventDeathData>     DeathEvents;
	TArray<FParticleEventCollideData>   CollisionEvents;
	TArray<FParticleEventBurstData>     BurstEvents;
};

// -----------------------------------------------------------------------------
// FParticleSpriteEmitterInstance / FParticleMeshEmitterInstance /
// FParticleBeamEmitterInstance / FParticleRibbonEmitterInstance
//   TypeData 모듈의 종류에 따라 emitter::CreateInstance() 가 선택해 만든다.
//   각 subclass 가 GetDynamicData() 에서 본인 type 의 ReplayData 를 채워 반환.
// -----------------------------------------------------------------------------
class FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
public:
	FDynamicEmitterDataBase* GetDynamicData() override;
};

class FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	FDynamicEmitterDataBase* GetDynamicData() override;
};

class FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
	FDynamicEmitterDataBase* GetDynamicData() override;
	// Beam 은 source/target 두 endpoint 가 필요. EventGenerator/외부에서 지정.
	void SetEndpoints(const FVector& InSource, const FVector& InTarget);
protected:
	FVector SourcePoint = { 0, 0, 0 };
	FVector TargetPoint = { 0, 0, 0 };
};

class FParticleRibbonEmitterInstance : public FParticleEmitterInstance
{
public:
	FDynamicEmitterDataBase* GetDynamicData() override;
};
