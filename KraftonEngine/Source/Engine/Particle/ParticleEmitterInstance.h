#pragma once

#include "Particle/ParticleHelper.h"
#include "Particle/ParticleEvents.h"
#include "Math/Transform.h"
#include "ParticleModule.h"
#include "ParticleLODLevel.h"
#include "Particle/Modules/ParticleModuleEventGenerator.h"

class UParticleEmitter;
class UParticleSystemComponent;
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

	// 런타임 instance type 식별 (RTTI/dynamic_cast 없이 분기·static_cast 용). 서브클래스가 override.
	virtual EDynamicEmitterType GetType() const { return EDynamicEmitterType::Unknown; }

	// 모듈 → payload offset 조회 헬퍼.
	uint32 GetModuleDataOffset(const UParticleModule* InModule) const;

	// 활성 입자 i 의 BaseParticle 포인터.
	FBaseParticle*       GetParticleAt(uint32 InActiveIndex);
	const FBaseParticle* GetParticleAt(uint32 InActiveIndex) const;
	// emitter-level instance payload block의 시작 주소. Spawn burst state 같이
	// "입자별이 아닌 emitter별 런타임 상태"를 여기에 둔다.
	void*                GetInstancePayloadData();
	const void*          GetInstancePayloadData() const;

	template<typename T>
	T* GetModuleInstancePayload(const UParticleModule* InModule)
	{
		if (!InModule) return nullptr;
		if (!RuntimeStorage.InstanceData) return nullptr;
		if (!ModuleInstanceOffsetMap) return nullptr;

		auto It = ModuleInstanceOffsetMap->find(InModule);
		if (It == ModuleInstanceOffsetMap->end()) return nullptr;
		return reinterpret_cast<T*>(RuntimeStorage.InstanceData + It->second);
	}

	template<typename T>
	const T* GetModuleInstancePayload(const UParticleModule* InModule) const
	{
		if (!InModule) return nullptr;
		if (!RuntimeStorage.InstanceData) return nullptr;
		if (!ModuleInstanceOffsetMap) return nullptr;

		auto It = ModuleInstanceOffsetMap->find(InModule);
		if (It == ModuleInstanceOffsetMap->end()) return nullptr;
		return reinterpret_cast<const T*>(RuntimeStorage.InstanceData + It->second);
	}

	template<typename Func>
	void ForEachActiveParticle(Func&& InFunc)
	{
		for (uint32 i = 0; i < ActiveParticles; ++i)
		{
			if (FBaseParticle* Particle = GetParticleAt(i))
			{
				InFunc(i, *Particle);
			}
		}
	}

	template<typename Func>
	void ForEachActiveParticleReverse(Func&& InFunc)
	{
		for (int32 i = static_cast<int32>(ActiveParticles) - 1; i >= 0; --i)
		{
			if (FBaseParticle* Particle = GetParticleAt(static_cast<uint32>(i)))
			{
				InFunc(static_cast<uint32>(i), *Particle);
			}
		}
	}

	uint32 GetActiveParticleCount() const { return ActiveParticles; }
	uint32 GetMaxParticleCount()    const { return MaxActiveParticles; }
	// finite loop emitter가 더 이상 spawn하지 않는 상태인지 / 실제로 완전히 끝났는지.
	bool   IsSpawningComplete()     const;
	bool   IsFinished()             const;
	// active particle 기준의 느슨한 world-space bounds. 없으면 false를 반환해
	// PSC가 template fixed bounds로 fallback 할 수 있게 한다.
	bool   ComputeDynamicBounds(FVector& OutMin, FVector& OutMax) const;
	// PSC가 선택한 현재 LOD를 런타임 instance에 전달하는 entry point.
	void   SetCurrentLODIndex(int32 InLODIndex);
	int32  GetCurrentLODIndex() const { return CurrentLODIndex; }

	// EventManager 가 디스패치할 큐. EventGenerator 모듈이 push.
	const TArray<FParticleEventSpawnData>&     GetSpawnEvents()     const { return SpawnEvents; }
	const TArray<FParticleEventDeathData>&     GetDeathEvents()     const { return DeathEvents; }
	const TArray<FParticleEventCollideData>&   GetCollisionEvents() const { return CollisionEvents; }
	const TArray<FParticleEventBurstData>&     GetBurstEvents()     const { return BurstEvents; }

	void ClearPendingEvents();
	void EnqueueSpawnEvent(const FParticleEventSpawnData& InEvent);
	void EnqueueDeathEvent(const FParticleEventDeathData& InEvent);
	void EnqueueCollisionEvent(const FParticleEventCollideData& InEvent);
	void EnqueueBurstEvent(const FParticleEventBurstData& InEvent);
	void EmitSpawnEvent(const FParticleEventSpawnData& InEvent);
	void EmitDeathEvent(const FParticleEventDeathData& InEvent);
	void EmitCollisionEvent(const FParticleEventCollideData& InEvent);
	void EmitBurstEvent(const FParticleEventBurstData& InEvent);

	UParticleEmitter*           GetEmitter()       const { return Emitter; }
	UParticleSystemComponent*   GetComponent()     const { return Component; }
	UParticleLODLevel*          GetCurrentLOD()    const;
	bool                        UsesLocalSpace()   const;
	// simulation space는 Required.bUseLocalSpace에 따라 local/world 중 하나로 고정된다.
	// spawn-time 모듈 입력을 이 공간으로 해석해 저장하면 GT/RT 좌표계 계약이 일관된다.
	FVector                     TransformLocalVectorToSimulation(const FVector& V) const;
	FVector                     TransformSimulationVectorToWorld(const FVector& V) const;
	FVector                     TransformWorldVectorToSimulation(const FVector& V) const;

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
	const class UParticleModuleRequired* GetRequiredModule() const;
	bool IsSpawningAllowed() const;
	void AdvanceLoopState(float DeltaTime);

	template<typename Func>
	void ForEachEventGenerator(Func&& InFunc) const
	{
		UParticleLODLevel* LOD = GetCurrentLOD();
		if (!LOD)
		{
			return;
		}

		for (UParticleModule* Module : LOD->Modules)
		{
			if (!Module || !Module->IsEnabled())
			{
				continue;
			}

			if (const auto* EventGenerator = Cast<UParticleModuleEventGenerator>(Module))
			{
				InFunc(EventGenerator);
			}
		}
	}

protected:
	UParticleEmitter*         Emitter     = nullptr;
	UParticleSystemComponent* Component   = nullptr;

	// 모듈 payload offset 캐시 — emitter 가 미리 계산해 둔 것을 가져와 빠른 조회.
	const TMap<const UParticleModule*, uint32>* ModuleOffsetMap = nullptr;
	const TMap<const UParticleModule*, uint32>* ModuleInstanceOffsetMap = nullptr;

	// 입자 buffer (BaseParticle + payload) flat layout.
	uint32           ParticleStride       = sizeof(FBaseParticle);
	uint32           MaxActiveParticles   = 0;
	uint32           ActiveParticles      = 0;
	FParticleStorage RuntimeStorage;

	// Spawn 누적 (분수 입자 carry-over)
	float SpawnFraction      = 0.0f;
	float EmitterTimeSeconds = 0.0f; // 누적 sim time
	float CurrentLoopTimeSeconds = 0.0f;
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
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Sprite; }
};

class FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	FDynamicEmitterDataBase* GetDynamicData() override;
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Mesh; }
};

class FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
	FDynamicEmitterDataBase* GetDynamicData() override;
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Beam; }
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
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Ribbon; }
};
