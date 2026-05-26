#include "ParticleEmitterInstance.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleCollision.h"
#include "Particle/Modules/ParticleModuleEventGenerator.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam.h"
#include "Particle/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/World.h"
#include "Math/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float ParticleCollisionMinTravelDistance = 1.0e-3f;
	constexpr float ParticleCollisionMinMeaningfulSpeed = 5.0e-2f;
	constexpr float ParticleCollisionCooldownSeconds = 2.5e-2f;
	constexpr float ParticleCollisionSameSurfaceNormalDotThreshold = 0.95f;
	constexpr float ParticleCollisionSurfaceOffset = 1.0e-2f;
	constexpr float ParticleCollisionBudgetPrioritySpeedWeight = 0.1f;
	constexpr float ParticleCollisionBudgetNearLimitPenalty = 0.5f;
	constexpr float ParticleCollisionBudgetRecentHitPenalty = 0.35f;
	constexpr float ParticleCollisionBudgetLowSpeedPenalty = 0.5f;
	constexpr float ParticleCollisionHighPriorityScoreThreshold = 20.0f;
	constexpr int32 ParticleCollisionBudgetLOD0 = 512;
	constexpr int32 ParticleCollisionBudgetLOD1 = 128;
	constexpr int32 ParticleCollisionBudgetLOD2OrLower = 0;

	FVector ReflectVelocityAboutNormal(const FVector& Velocity, const FVector& Normal)
	{
		return Velocity - Normal * (2.0f * Velocity.Dot(Normal));
	}

	bool HasCollisionCountLimit(const UParticleModuleCollision& CollisionModule)
	{
		return CollisionModule.MaxCollisions > 0;
	}

	float GetCollisionSpeed(const FVector& Velocity)
	{
		return Velocity.Length();
	}

	bool IsMeaningfulCollisionSpeed(float Speed)
	{
		return Speed > ParticleCollisionMinMeaningfulSpeed;
	}

	bool WasCollisionHandledRecently(
		const UParticleModuleCollision::FCollisionParticlePayload& Payload,
		float EvaluationTimeSeconds)
	{
		return Payload.LastCollisionTime >= 0.0f &&
			(EvaluationTimeSeconds - Payload.LastCollisionTime) <= ParticleCollisionCooldownSeconds;
	}

	UParticleModuleCollision::ECollisionResponseMode ResolveImmediateCollisionResponseMode(
		const UParticleModuleCollision& CollisionModule)
	{
		if (CollisionModule.bKillOnCollision)
		{
			return UParticleModuleCollision::ECollisionResponseMode::Kill;
		}

		return CollisionModule.ResponseMode;
	}

	void ApplyCollisionCompletionBehavior(
		FBaseParticle& Particle,
		UParticleModuleCollision::FCollisionParticlePayload& Payload,
		const UParticleModuleCollision& CollisionModule)
	{
		switch (CollisionModule.CompletionMode)
		{
		case UParticleModuleCollision::ECollisionCompletionMode::Kill:
			Particle.Flags |= static_cast<uint32>(EParticleStateFlags::Killed);
			break;
		case UParticleModuleCollision::ECollisionCompletionMode::IgnoreFurtherCollisions:
			Payload.bIgnoreFurtherCollisions = true;
			break;
		case UParticleModuleCollision::ECollisionCompletionMode::Freeze:
		default:
			Payload.bIgnoreFurtherCollisions = true;
			Payload.bFrozenAfterLimit = true;
			Particle.Velocity = FVector::ZeroVector;
			Particle.OldLocation = Particle.Location;
			break;
		}
	}

	bool ShouldSuppressRepeatedCollisionHit(
		const UParticleModuleCollision::FCollisionParticlePayload& Payload,
		const FVector& CollisionNormal,
		float CollisionTimeSeconds)
	{
		if (Payload.LastCollisionTime < 0.0f)
		{
			return false;
		}

		const float TimeSinceLastCollision = CollisionTimeSeconds - Payload.LastCollisionTime;
		if (TimeSinceLastCollision > ParticleCollisionCooldownSeconds)
		{
			return false;
		}

		const FVector PriorNormal = Payload.LastCollisionNormal.IsNearlyZero()
			? FVector::UpVector
			: Payload.LastCollisionNormal.Normalized();
		const float SameSurfaceDot = CollisionNormal.Dot(PriorNormal);
		return SameSurfaceDot >= ParticleCollisionSameSurfaceNormalDotThreshold;
	}

	void UpdateRecentCollisionState(
		UParticleModuleCollision::FCollisionParticlePayload& Payload,
		float CollisionTimeSeconds,
		const FVector& CollisionNormal)
	{
		Payload.LastCollisionTime = CollisionTimeSeconds;
		Payload.LastCollisionNormal = CollisionNormal;
	}
}

static EParticleReplaySortMode ToReplaySortMode(UParticleModuleRequired::ESortMode InSortMode)
{
	// RequiredModule의 UObject enum을 replay 전용 POD enum으로 축소.
	// RT는 이 변환 결과만 보고 정렬 comparator를 선택한다.
	switch (InSortMode)
	{
	case UParticleModuleRequired::ESortMode::ViewProjDepth:
		return EParticleReplaySortMode::ViewProjDepth;
	case UParticleModuleRequired::ESortMode::ViewDistance:
		return EParticleReplaySortMode::ViewDistance;
	case UParticleModuleRequired::ESortMode::Age_OldestFirst:
		return EParticleReplaySortMode::Age_OldestFirst;
	case UParticleModuleRequired::ESortMode::Age_NewestFirst:
		return EParticleReplaySortMode::Age_NewestFirst;
	case UParticleModuleRequired::ESortMode::None:
	default:
		return EParticleReplaySortMode::None;
	}
}

static EParticleMeshReplayAlignment ToReplayMeshAlignment(
	UParticleModuleTypeDataMesh::EMeshAlignment InAlignment)
{
	switch (InAlignment)
	{
	case UParticleModuleTypeDataMesh::EMeshAlignment::Velocity:
		return EParticleMeshReplayAlignment::Velocity;
	case UParticleModuleTypeDataMesh::EMeshAlignment::FacingCamera:
		return EParticleMeshReplayAlignment::FacingCamera;
	case UParticleModuleTypeDataMesh::EMeshAlignment::AxisLock:
		return EParticleMeshReplayAlignment::AxisLock;
	case UParticleModuleTypeDataMesh::EMeshAlignment::None:
	default:
		return EParticleMeshReplayAlignment::None;
	}
}

static EParticleSpriteReplayAlignment ToReplaySpriteAlignment(
	UParticleModuleRequired::EScreenAlignment InAlignment)
{
	switch (InAlignment)
	{
	case UParticleModuleRequired::EScreenAlignment::Rectangle:
		return EParticleSpriteReplayAlignment::Rectangle;
	case UParticleModuleRequired::EScreenAlignment::Velocity:
		return EParticleSpriteReplayAlignment::Velocity;
	case UParticleModuleRequired::EScreenAlignment::FacingCameraPosition:
		return EParticleSpriteReplayAlignment::FacingCameraPosition;
	case UParticleModuleRequired::EScreenAlignment::Square:
	default:
		return EParticleSpriteReplayAlignment::Square;
	}
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	RuntimeStorage.Release();
}

void FParticleEmitterInstance::Init(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent)
{
	Emitter = InEmitter;
	Component = InComponent;

	CurrentLODIndex = 0;
	SpawnFraction = 0.0f;
	EmitterTimeSeconds = 0.0f;
	CurrentLoopTimeSeconds = 0.0f;
	LoopCount = 0;
	ActiveParticles = 0;

	ClearPendingEvents();

	if (Emitter)
	{
		Emitter->InitializeDefaultLODLevel();
		Emitter->CacheEmitterModuleInfo();

		ModuleOffsetMap = &Emitter->GetModuleOffsetMap();
		ModuleInstanceOffsetMap = &Emitter->GetParticleLayout().InstanceModuleOffsets;

		ParticleStride = std::max<uint32>(Emitter->GetParticleSize(), sizeof(FBaseParticle));
	}
	else
	{
		ModuleOffsetMap = nullptr;
		ModuleInstanceOffsetMap = nullptr;
		ParticleStride = sizeof(FBaseParticle);
	}

	ResizeParticleData(GetInitialParticleCapacity());
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (!Emitter) return;
	if (!Component) return;
	if (DeltaTime <= 0.0f) return;

	ClearPendingEvents();

	float RemainingDeltaTime = DeltaTime;
	// duration이 있는 emitter는 loop 경계에서 step을 잘라서 처리해야
	// SpawnModule의 burst/time 누적과 LoopCount 갱신이 같은 프레임 안에서도 일관된다.
	while (RemainingDeltaTime > 0.0f)
	{
		float StepDeltaTime = RemainingDeltaTime;

		if (const UParticleModuleRequired* Required = GetRequiredModule())
		{
			if (Required->EmitterDuration > 0.0f && IsSpawningAllowed())
			{
				const float TimeUntilLoopEnd =
					std::max(0.0f, Required->EmitterDuration - CurrentLoopTimeSeconds);
				StepDeltaTime = std::min(StepDeltaTime, TimeUntilLoopEnd);
			}
		}

		if (StepDeltaTime <= 0.0f)
		{
			AdvanceLoopState(0.0f);
			break;
		}

		if (IsSpawningAllowed())
		{
			SpawnParticles(StepDeltaTime);
		}

		UpdateParticles(StepDeltaTime);
		AdvanceLoopState(StepDeltaTime);
		RemainingDeltaTime -= StepDeltaTime;
	}
}

void FParticleEmitterInstance::Reset()
{
	ActiveParticles = 0;
	SpawnFraction = 0.0f;
	EmitterTimeSeconds = 0.0f;
	CurrentLoopTimeSeconds = 0.0f;
	LoopCount = 0;

	ClearPendingEvents();

	for (uint32 i = 0; i < MaxActiveParticles; ++i)
	{
		RuntimeStorage.ParticleIndices[i] = static_cast<uint16>(i);
	}

	if (RuntimeStorage.InstanceData && RuntimeStorage.InstanceDataBytes > 0)
	{
		std::memset(RuntimeStorage.InstanceData, 0, RuntimeStorage.InstanceDataBytes);
	}
}

FDynamicEmitterDataBase* FParticleEmitterInstance::GetDynamicData()
{
	// 베이스는 데이터 만들지 않음. type-별 서브클래스가 구현.
	return nullptr;
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(const UParticleModule* InModule) const
{
	if (!ModuleOffsetMap) return 0;
	auto It = ModuleOffsetMap->find(InModule);
	if (It == ModuleOffsetMap->end()) return 0;
	return It->second;
}

FBaseParticle* FParticleEmitterInstance::GetParticleAt(uint32 InActiveIndex)
{
	if (InActiveIndex >= ActiveParticles) return nullptr;
	const uint16 Slot = RuntimeStorage.ParticleIndices[InActiveIndex];
	return reinterpret_cast<FBaseParticle*>(RuntimeStorage.ParticleData + Slot * ParticleStride);
}

const FBaseParticle* FParticleEmitterInstance::GetParticleAt(uint32 InActiveIndex) const
{
	if (InActiveIndex >= ActiveParticles) return nullptr;
	const uint16 Slot = RuntimeStorage.ParticleIndices[InActiveIndex];
	return reinterpret_cast<const FBaseParticle*>(RuntimeStorage.ParticleData + Slot * ParticleStride);
}

void* FParticleEmitterInstance::GetInstancePayloadData()
{
	return RuntimeStorage.InstanceData;
}

const void* FParticleEmitterInstance::GetInstancePayloadData() const
{
	return RuntimeStorage.InstanceData;
}

UParticleLODLevel* FParticleEmitterInstance::GetCurrentLOD() const
{
	if (!Emitter) return nullptr;
	return Emitter->GetCurrentLODLevel(CurrentLODIndex);
}

bool FParticleEmitterInstance::UsesLocalSpace() const
{
	const UParticleModuleRequired* Required = GetRequiredModule();
	return Required ? Required->bUseLocalSpace : false;
}

FVector FParticleEmitterInstance::ConvertVectorToSimulation(
	const FVector& V,
	EParticleValueSpace SourceSpace) const
{
	switch (SourceSpace)
	{
	case EParticleValueSpace::Simulation:
		return V;
	case EParticleValueSpace::Local:
		if (UsesLocalSpace() || !Component)
		{
			return V;
		}

		return Component->GetWorldMatrix().TransformVector(V);
	case EParticleValueSpace::World:
		if (!UsesLocalSpace() || !Component)
		{
			return V;
		}

		return Component->GetWorldInverseMatrix().TransformVector(V);
	default:
		return V;
	}
}

FVector FParticleEmitterInstance::ConvertVectorFromSimulation(
	const FVector& V,
	EParticleValueSpace TargetSpace) const
{
	switch (TargetSpace)
	{
	case EParticleValueSpace::Simulation:
		return V;
	case EParticleValueSpace::Local:
		if (UsesLocalSpace() || !Component)
		{
			return V;
		}

		return Component->GetWorldInverseMatrix().TransformVector(V);
	case EParticleValueSpace::World:
		if (!UsesLocalSpace() || !Component)
		{
			return V;
		}

		return Component->GetWorldMatrix().TransformVector(V);
	default:
		return V;
	}
}

FVector FParticleEmitterInstance::ConvertPositionToSimulation(
	const FVector& P,
	EParticleValueSpace SourceSpace) const
{
	switch (SourceSpace)
	{
	case EParticleValueSpace::Simulation:
		return P;
	case EParticleValueSpace::Local:
		if (UsesLocalSpace() || !Component)
		{
			return P;
		}

		return Component->GetWorldMatrix().TransformPositionWithW(P);
	case EParticleValueSpace::World:
		if (!UsesLocalSpace() || !Component)
		{
			return P;
		}

		return Component->GetWorldInverseMatrix().TransformPositionWithW(P);
	default:
		return P;
	}
}

FVector FParticleEmitterInstance::ConvertPositionFromSimulation(
	const FVector& P,
	EParticleValueSpace TargetSpace) const
{
	switch (TargetSpace)
	{
	case EParticleValueSpace::Simulation:
		return P;
	case EParticleValueSpace::Local:
		if (UsesLocalSpace() || !Component)
		{
			return P;
		}

		return Component->GetWorldInverseMatrix().TransformPositionWithW(P);
	case EParticleValueSpace::World:
		if (!UsesLocalSpace() || !Component)
		{
			return P;
		}

		return Component->GetWorldMatrix().TransformPositionWithW(P);
	default:
		return P;
	}
}

void FParticleEmitterInstance::SetCurrentLODIndex(int32 InLODIndex)
{
	CurrentLODIndex = std::max(0, InLODIndex);
}

bool FParticleEmitterInstance::IsSpawningComplete() const
{
	const UParticleModuleRequired* Required = GetRequiredModule();
	if (!Required)
	{
		return false;
	}

	if (Required->EmitterDuration <= 0.0f || Required->EmitterLoops <= 0)
	{
		return false;
	}

	return LoopCount >= Required->EmitterLoops;
}

bool FParticleEmitterInstance::IsFinished() const
{
	return IsSpawningComplete() && ActiveParticles == 0;
}

bool FParticleEmitterInstance::ComputeDynamicBounds(FVector& OutMin, FVector& OutMax) const
{
	if (ActiveParticles == 0)
	{
		return false;
	}

	const UParticleModuleRequired* Required = GetRequiredModule();
	const bool bUseLocalSpace = Required ? Required->bUseLocalSpace : false;
	const FMatrix LocalToWorld = Component ? Component->GetWorldMatrix() : FMatrix{};

	bool bHasBounds = false;
	// 현재는 sprite/mesh 공통으로 Location +/- Size 의 느슨한 근사 박스를 사용.
	// culling/selection 용으로는 충분하고, 정확한 mesh bounds는 추후 확장 가능.
	for (uint32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAt(i);
		if (!Particle)
		{
			continue;
		}

		FVector Position = Particle->Location;
		if (bUseLocalSpace && Component)
		{
			Position = LocalToWorld.TransformPositionWithW(Position);
		}

		const FVector Extent{
			std::abs(Particle->Size.X),
			std::abs(Particle->Size.Y),
			std::abs(Particle->Size.Z)
		};

		const FVector ParticleMin = Position - Extent;
		const FVector ParticleMax = Position + Extent;

		if (!bHasBounds)
		{
			OutMin = ParticleMin;
			OutMax = ParticleMax;
			bHasBounds = true;
			continue;
		}

		OutMin.X = std::min(OutMin.X, ParticleMin.X);
		OutMin.Y = std::min(OutMin.Y, ParticleMin.Y);
		OutMin.Z = std::min(OutMin.Z, ParticleMin.Z);
		OutMax.X = std::max(OutMax.X, ParticleMax.X);
		OutMax.Y = std::max(OutMax.Y, ParticleMax.Y);
		OutMax.Z = std::max(OutMax.Z, ParticleMax.Z);
	}

	return bHasBounds;
}

FTransform FParticleEmitterInstance::GetComponentToWorld() const
{
	if (!Component) return FTransform{};
	return FTransform(Component->GetWorldMatrix());
}

void FParticleEmitterInstance::ClearPendingEvents()
{
	SpawnEvents.clear();
	DeathEvents.clear();
	CollisionEvents.clear();
	BurstEvents.clear();
}

void FParticleEmitterInstance::EnqueueSpawnEvent(const FParticleEventSpawnData& InEvent)
{
	SpawnEvents.push_back(InEvent);
}

void FParticleEmitterInstance::EnqueueDeathEvent(const FParticleEventDeathData& InEvent)
{
	DeathEvents.push_back(InEvent);
}

void FParticleEmitterInstance::EnqueueCollisionEvent(const FParticleEventCollideData& InEvent)
{
	CollisionEvents.push_back(InEvent);
}

void FParticleEmitterInstance::EnqueueBurstEvent(const FParticleEventBurstData& InEvent)
{
	BurstEvents.push_back(InEvent);
}

void FParticleEmitterInstance::EmitSpawnEvent(const FParticleEventSpawnData& InEvent)
{
	EnqueueSpawnEvent(InEvent);

	ForEachEventGenerator([this, &InEvent](const UParticleModuleEventGenerator* EventGenerator)
		{
			EventGenerator->HandleSpawnEvent(this, InEvent);
		});
}

void FParticleEmitterInstance::EmitDeathEvent(const FParticleEventDeathData& InEvent)
{
	EnqueueDeathEvent(InEvent);

	ForEachEventGenerator([this, &InEvent](const UParticleModuleEventGenerator* EventGenerator)
		{
			EventGenerator->HandleDeathEvent(this, InEvent);
		});
}

void FParticleEmitterInstance::EmitCollisionEvent(const FParticleEventCollideData& InEvent)
{
	EnqueueCollisionEvent(InEvent);

	ForEachEventGenerator([this, &InEvent](const UParticleModuleEventGenerator* EventGenerator)
		{
			EventGenerator->HandleCollisionEvent(this, InEvent);
		});
}

void FParticleEmitterInstance::EmitBurstEvent(const FParticleEventBurstData& InEvent)
{
	EnqueueBurstEvent(InEvent);

	ForEachEventGenerator([this, &InEvent](const UParticleModuleEventGenerator* EventGenerator)
		{
			EventGenerator->HandleBurstEvent(this, InEvent);
		});
}

void FParticleEmitterInstance::SpawnParticles(float DeltaTime)
{
	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->bEnabled) return;
	if (!LOD->SpawnModule) return;
	if (!IsSpawningAllowed()) return;

	UParticleModuleSpawn* SpawnModule = LOD->SpawnModule;

	float SpawnAmount = 0.0f;

	// SpawnTime은 particle relative time이 아니라 현재 emitter loop 안의 시간이다.
	// SpawnModule은 Rate/RateScale 계산만 맡고, BurstList의 실제 firing은 emitter instance가 처리한다.
	SpawnModule->GetRateSpawnAmount(
		this,
		DeltaTime,
		CurrentLoopTimeSeconds,
		SpawnAmount);

	SpawnAmount = std::max(0.0f, SpawnAmount);

	const float TotalSpawnFloat = SpawnFraction + SpawnAmount;
	const int32 RateSpawnCount = static_cast<int32>(std::floor(TotalSpawnFloat));
	SpawnFraction = TotalSpawnFloat - static_cast<float>(RateSpawnCount);

	int32 SpawnBudget = static_cast<int32>(
		ParticleConstants::MaxParticlesPerEmitter > ActiveParticles
		? ParticleConstants::MaxParticlesPerEmitter - ActiveParticles
		: 0);

	if (SpawnBudget <= 0) return;

	SpawnBudget = std::min(
		SpawnBudget,
		static_cast<int32>(ParticleConstants::MaxBurstCountPerFrame));

	// Burst는 정해진 loop time에 터지는 이벤트성이 강하므로 Rate보다 먼저 처리한다.
	SpawnBurstParticles(SpawnModule, DeltaTime, SpawnBudget);

	if (RateSpawnCount > 0 && SpawnBudget > 0)
	{
		const int32 Count = std::min(RateSpawnCount, SpawnBudget);

		const float Increment =
			Count > 0
			? DeltaTime / static_cast<float>(Count)
			: 0.0f;

		// 각 spawn 구간의 중앙점에 배치한다.
		// 예: Count=4, Delta=0.016 -> 0.002, 0.006, 0.010, 0.014초 지점.
		const float StartTime = CurrentLoopTimeSeconds + Increment * 0.5f;

		SpawnInternal(Count, StartTime, Increment, DeltaTime);
	}
}

int32 FParticleEmitterInstance::SpawnBurstParticles(UParticleModuleSpawn* SpawnModule, float DeltaTime, int32& InOutSpawnBudget)
{
	if (!SpawnModule || InOutSpawnBudget <= 0)
	{
		return 0;
	}

	int32 TotalSpawned = 0;

	const float SafeDeltaTime = std::max(0.0f, DeltaTime);
	const float CurrentTime = CurrentLoopTimeSeconds + SafeDeltaTime;
	float PreviousTime = CurrentLoopTimeSeconds;

	if (UParticleModuleSpawn::FSpawnModuleInstancePayload* Payload =
		GetModuleInstancePayload<UParticleModuleSpawn::FSpawnModuleInstancePayload>(SpawnModule))
	{
		PreviousTime = Payload->LastProcessedBurstTime;
		if (CurrentTime < PreviousTime)
		{
			PreviousTime = CurrentLoopTimeSeconds;
		}

		Payload->LastProcessedBurstTime = CurrentTime;
	}

	for (const UParticleModuleSpawn::FBurstEntry& Entry : SpawnModule->BurstList)
	{
		if (InOutSpawnBudget <= 0) break;

		if (Entry.Count <= 0)
		{
			continue;
		}

		if (Entry.Time < PreviousTime || Entry.Time >= CurrentTime)
		{
			continue;
		}

		const int32 Count = std::min(Entry.Count, InOutSpawnBudget);
		if (Count <= 0) continue;

		const int32 SpawnedCount = SpawnInternal(Count, Entry.Time, 0.0f, SafeDeltaTime);
		InOutSpawnBudget -= SpawnedCount;
		TotalSpawned += SpawnedCount;

		if (SpawnedCount > 0)
		{
			const float BurstOffsetSeconds = std::clamp(
				Entry.Time - CurrentLoopTimeSeconds,
				0.0f,
				SafeDeltaTime);

			FParticleEventBurstData BurstEvent;
			BurstEvent.Type = EParticleEventType::Burst;
			BurstEvent.TimeSeconds = EmitterTimeSeconds + BurstOffsetSeconds;
			BurstEvent.ParticleCount = SpawnedCount;

			if (Component)
			{
				BurstEvent.Location = Component->GetWorldLocation();
			}

			EmitBurstEvent(BurstEvent);
		}
	}

	return TotalSpawned;
}

int32 FParticleEmitterInstance::SpawnInternal(int32 Count, float StartTime, float Increment, float StepDeltaTime)
{
	if (Count <= 0) return 0;

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->bEnabled) return 0;

	const uint32 RequiredActiveCount = ActiveParticles + static_cast<uint32>(Count);
	if (RequiredActiveCount > MaxActiveParticles)
	{
		const uint32 NewCapacity = GrowParticleCapacity(MaxActiveParticles, RequiredActiveCount);
		if (NewCapacity <= MaxActiveParticles) return 0;

		ResizeParticleData(NewCapacity);
	}

	const uint32 ActiveFlag = static_cast<uint32>(EParticleStateFlags::Active);
	const uint32 SpawnedFlag = static_cast<uint32>(EParticleStateFlags::Spawned);

	int32 SpawnedCount = 0;

	for (int32 SpawnIndex = 0; SpawnIndex < Count; ++SpawnIndex)
	{
		if (ActiveParticles >= MaxActiveParticles) break;

		const float SpawnTime = StartTime + Increment * static_cast<float>(SpawnIndex);
		const float SpawnOffsetSeconds = std::clamp(
			SpawnTime - CurrentLoopTimeSeconds,
			0.0f,
			StepDeltaTime);
		const float AbsoluteSpawnTime = EmitterTimeSeconds + SpawnOffsetSeconds;

		const uint32 Slot = ActiveParticles;
		uint8* ParticleBytes = RuntimeStorage.ParticleData + Slot * ParticleStride;

		std::memset(ParticleBytes, 0, ParticleStride);

		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleBytes);
		*Particle = FBaseParticle{};

		Particle->Flags = ActiveFlag | SpawnedFlag;

		bool bUseLocalSpace = false;

		if (LOD && LOD->RequiredModule)
		{
			bUseLocalSpace = LOD->RequiredModule->bUseLocalSpace;
		}

		if (Component)
		{
			if (bUseLocalSpace) Particle->Location = FVector{ 0, 0, 0 };
			else Particle->Location = Component->GetWorldLocation();

			Particle->OldLocation = Particle->Location;
		}

		RuntimeStorage.ParticleIndices[ActiveParticles] = static_cast<uint16>(Slot);
		++ActiveParticles;
		++SpawnedCount;

		for (UParticleModule* Module : LOD->Modules)
		{
			if (!Module || !Module->IsEnabled()) continue;

			const uint32 ModuleOffset = GetModuleDataOffset(Module);
			// SpawnTime은 emitter-loop 기준 seconds이다.
			// Initial Distribution은 이 값을 쓰고, Over-Life 모듈은 Particle->RelativeTime을 쓴다.
			Module->Spawn(this, ModuleOffset, SpawnTime, Particle);
		}

		FParticleEventSpawnData SpawnEvent;
		SpawnEvent.Type = EParticleEventType::Spawn;
		SpawnEvent.TimeSeconds = AbsoluteSpawnTime;
		SpawnEvent.Location = Particle->Location;
		SpawnEvent.Velocity = Particle->Velocity;
		SpawnEvent.ParticleCount = 1;
		EmitSpawnEvent(SpawnEvent);

		// 현재 tick 순서는 Spawn -> Update다.
		// sub-frame spawn을 흉내 내기 위해 spawn 이전 시간만큼 위치를 뒤로 보정한다.
		if (SpawnOffsetSeconds > 0.0f)
		{
			Particle->Location = Particle->Location - Particle->Velocity * SpawnOffsetSeconds;
			Particle->OldLocation = Particle->Location;
		}
	}

	return SpawnedCount;
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->bEnabled) return;

	for (uint32 i = 0; i < ActiveParticles; ++i)
	{
		FBaseParticle* Particle = GetParticleAt(i);
		if (!Particle) continue;

		Particle->OldLocation = Particle->Location;
		Particle->Location = Particle->Location + Particle->Velocity * DeltaTime;
		Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;
	}

	for (UParticleModule* Module : LOD->Modules)
	{
		if (!Module || !Module->IsEnabled()) continue;

		const uint32 ModuleOffset = GetModuleDataOffset(Module);
		Module->Update(this, ModuleOffset, DeltaTime);
	}

	ResolveParticleCollisions(DeltaTime);

	uint32 WriteIndex = 0;

	for (uint32 ReadIndex = 0; ReadIndex < ActiveParticles; ++ReadIndex)
	{
		FBaseParticle* Particle = GetParticleAt(ReadIndex);
		if (!Particle) continue;

		if (IsParticleKilled(Particle))
		{
			FParticleEventDeathData DeathEvent;
			DeathEvent.Type = EParticleEventType::Death;
			DeathEvent.TimeSeconds = EmitterTimeSeconds;
			DeathEvent.Location = Particle->Location;
			DeathEvent.Velocity = Particle->Velocity;
			DeathEvent.ParticleAge = Particle->RelativeTime;
			EmitDeathEvent(DeathEvent);

			continue;
		}

		ClearSpawnedFlag(Particle);

		if (WriteIndex != ReadIndex)
		{
			FBaseParticle* WriteParticle =
				reinterpret_cast<FBaseParticle*>(RuntimeStorage.ParticleData + WriteIndex * ParticleStride);

			std::memmove(WriteParticle, Particle, ParticleStride);
		}

		RuntimeStorage.ParticleIndices[WriteIndex] = static_cast<uint16>(WriteIndex);
		++WriteIndex;
	}

	ActiveParticles = WriteIndex;
}

void FParticleEmitterInstance::ResolveParticleCollisions(float DeltaTime)
{
	const UParticleModuleCollision* CollisionModule = GetCollisionModule();
	if (!CollisionModule)
	{
		return;
	}

	UWorld* World = Component ? Component->GetWorld() : nullptr;
	if (!World || ActiveParticles == 0)
	{
		return;
	}

	if (!ShouldProcessCollisionsForCurrentLOD())
	{
		return;
	}

	if (!ModuleOffsetMap)
	{
		return;
	}

	const auto OffsetIt = ModuleOffsetMap->find(CollisionModule);
	if (OffsetIt == ModuleOffsetMap->end())
	{
		return;
	}

	const uint32 CollisionModuleOffset = OffsetIt->second;

	const int32 CollisionBudget = GetCollisionCheckBudgetForCurrentLOD();
	if (CollisionBudget <= 0)
	{
		return;
	}

	TArray<uint32> HighPriorityCandidates;
	TArray<uint32> FallbackCandidates;
	HighPriorityCandidates.reserve(ActiveParticles);
	FallbackCandidates.reserve(ActiveParticles);

	ForEachActiveParticle([this, CollisionModule, CollisionModuleOffset, DeltaTime, &HighPriorityCandidates, &FallbackCandidates](uint32 ActiveIndex, FBaseParticle& Particle)
		{
			// Budget is no longer "first N particles win". We first remove obvious
			// no-query cases, then spend the limited collision work on the more
			// meaningful movers before falling back to lower-value candidates.
			if (FinalizeParticleCollisionWithoutQuery(Particle, *CollisionModule, CollisionModuleOffset))
			{
				return;
			}

			if (ShouldSkipParticleCollisionForBudget(Particle, *CollisionModule, CollisionModuleOffset, DeltaTime))
			{
				return;
			}

			const float PriorityScore =
				GetParticleCollisionPriorityScore(Particle, *CollisionModule, CollisionModuleOffset, DeltaTime);

			if (IsHighPriorityCollisionCandidate(PriorityScore))
			{
				HighPriorityCandidates.push_back(ActiveIndex);
				return;
			}

			FallbackCandidates.push_back(ActiveIndex);
		});

	int32 CollisionChecksRemaining = CollisionBudget;
	auto ProcessCollisionCandidates = [this, CollisionModule, CollisionModuleOffset, DeltaTime, &CollisionChecksRemaining](const TArray<uint32>& CandidateIndices)
		{
			for (uint32 ActiveIndex : CandidateIndices)
			{
				if (CollisionChecksRemaining <= 0)
				{
					return;
				}

				FBaseParticle* Particle = GetParticleAt(ActiveIndex);
				if (!Particle)
				{
					continue;
				}

				if (FinalizeParticleCollisionWithoutQuery(*Particle, *CollisionModule, CollisionModuleOffset))
				{
					continue;
				}

				--CollisionChecksRemaining;
				ResolveSingleParticleCollision(*Particle, *CollisionModule, CollisionModuleOffset, DeltaTime);
			}
		};

	ProcessCollisionCandidates(HighPriorityCandidates);
	// Any remaining candidates beyond the current budget are "not processed this
	// tick" because of budget policy. That is distinct from processing a hit and
	// then suppressing its event as repeated-contact noise or lower-LOD gating.
	ProcessCollisionCandidates(FallbackCandidates);
}

const UParticleModuleCollision* FParticleEmitterInstance::GetCollisionModule() const
{
	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD)
	{
		return nullptr;
	}

	for (UParticleModule* Module : LOD->Modules)
	{
		if (!Module || !Module->IsEnabled())
		{
			continue;
		}

		if (const auto* CollisionModule = Cast<UParticleModuleCollision>(Module))
		{
			return CollisionModule;
		}
	}

	return nullptr;
}

bool FParticleEmitterInstance::FinalizeParticleCollisionWithoutQuery(
	FBaseParticle& Particle,
	const UParticleModuleCollision& CollisionModule,
	uint32 ModuleOffset) const
{
	const uint32 KilledFlag = static_cast<uint32>(EParticleStateFlags::Killed);
	if ((Particle.Flags & KilledFlag) != 0)
	{
		return true;
	}

	auto* Payload =
		PARTICLE_PAYLOAD(&Particle, ModuleOffset, UParticleModuleCollision::FCollisionParticlePayload);
	if (!Payload)
	{
		return true;
	}

	Payload->NumCollisions = std::max(0, Payload->NumCollisions);
	if (Payload->bFrozenAfterLimit)
	{
		// Freeze is a long-lived completion state. We keep restoring it without
		// spending query budget so low-value settled particles do not crowd out
		// faster, still-meaningful collision candidates.
		Particle.Location = Particle.OldLocation;
		Particle.Velocity = FVector::ZeroVector;
		return true;
	}

	if (Payload->bIgnoreFurtherCollisions)
	{
		return true;
	}

	if (HasCollisionCountLimit(CollisionModule) && Payload->NumCollisions >= CollisionModule.MaxCollisions)
	{
		ApplyCollisionCompletionBehavior(Particle, *Payload, CollisionModule);
		return true;
	}

	return false;
}

bool FParticleEmitterInstance::ShouldSkipParticleCollisionForBudget(
	const FBaseParticle& Particle,
	const UParticleModuleCollision& CollisionModule,
	uint32 ModuleOffset,
	float DeltaTime) const
{
	const auto* Payload =
		PARTICLE_PAYLOAD_CONST(&Particle, ModuleOffset, UParticleModuleCollision::FCollisionParticlePayload);
	if (!Payload)
	{
		return true;
	}

	const FVector StartWorld = ConvertPositionFromSimulation(Particle.OldLocation, EParticleValueSpace::World);
	const FVector EndWorld = ConvertPositionFromSimulation(Particle.Location, EParticleValueSpace::World);
	const float TravelDistance = (EndWorld - StartWorld).Length();
	if (TravelDistance <= ParticleCollisionMinTravelDistance)
	{
		return true;
	}

	const float CollisionSpeed = GetCollisionSpeed(
		ConvertVectorFromSimulation(Particle.Velocity, EParticleValueSpace::World));
	if (!IsMeaningfulCollisionSpeed(CollisionSpeed) &&
		WasCollisionHandledRecently(*Payload, EmitterTimeSeconds + DeltaTime))
	{
		// Very low-speed particles that just interacted recently are likely to hit
		// the repeated-contact stabilizer again. Skip spending scarce query budget
		// on them so faster movers get first claim.
		return true;
	}

	if (HasCollisionCountLimit(CollisionModule))
	{
		const int32 RemainingCollisions = CollisionModule.MaxCollisions - Payload->NumCollisions;
		if (RemainingCollisions <= 0)
		{
			return true;
		}
	}

	return false;
}

float FParticleEmitterInstance::GetParticleCollisionPriorityScore(
	const FBaseParticle& Particle,
	const UParticleModuleCollision& CollisionModule,
	uint32 ModuleOffset,
	float DeltaTime) const
{
	const auto* Payload =
		PARTICLE_PAYLOAD_CONST(&Particle, ModuleOffset, UParticleModuleCollision::FCollisionParticlePayload);
	if (!Payload)
	{
		return 0.0f;
	}

	const FVector StartWorld = ConvertPositionFromSimulation(Particle.OldLocation, EParticleValueSpace::World);
	const FVector EndWorld = ConvertPositionFromSimulation(Particle.Location, EParticleValueSpace::World);
	const float TravelDistance = (EndWorld - StartWorld).Length();
	const float CollisionSpeed = GetCollisionSpeed(
		ConvertVectorFromSimulation(Particle.Velocity, EParticleValueSpace::World));

	float PriorityScore =
		TravelDistance + (CollisionSpeed * ParticleCollisionBudgetPrioritySpeedWeight);
	// Current LOD still decides the outer collision budget envelope. This score
	// only decides how to spend that remaining budget more selectively inside the
	// explicit CPU collision pass.

	if (WasCollisionHandledRecently(*Payload, EmitterTimeSeconds + DeltaTime))
	{
		PriorityScore *= ParticleCollisionBudgetRecentHitPenalty;
	}

	if (!IsMeaningfulCollisionSpeed(CollisionSpeed))
	{
		PriorityScore *= ParticleCollisionBudgetLowSpeedPenalty;
	}

	if (HasCollisionCountLimit(CollisionModule))
	{
		const int32 RemainingCollisions = CollisionModule.MaxCollisions - Payload->NumCollisions;
		if (RemainingCollisions <= 1)
		{
			PriorityScore *= ParticleCollisionBudgetNearLimitPenalty;
		}
	}

	return PriorityScore;
}

bool FParticleEmitterInstance::IsHighPriorityCollisionCandidate(float PriorityScore) const
{
	return PriorityScore >= ParticleCollisionHighPriorityScoreThreshold;
}

bool FParticleEmitterInstance::ShouldEmitCollisionEventForAcceptedHit(
	const UParticleModuleCollision& CollisionModule) const
{
	// Event eligibility is a reporting policy layer on top of accepted collision
	// processing. A collision can still be processed even when the event report is
	// gated off by module settings or lower-LOD event fidelity policy.
	return CollisionModule.bGenerateCollisionEvents &&
		ShouldEmitCollisionEventsForCurrentLOD();
}

void FParticleEmitterInstance::EmitCollisionEventForAcceptedHit(
	const FBaseParticle& Particle,
	const FVector& CollisionNormal,
	const FVector& ImpactVelocityWorld,
	const FHitResult& Hit,
	float CollisionTimeSeconds)
{
	FParticleEventCollideData CollisionEvent;
	CollisionEvent.Type = EParticleEventType::Collision;
	CollisionEvent.TimeSeconds = CollisionTimeSeconds;
	CollisionEvent.Location = Hit.WorldHitLocation;
	CollisionEvent.Velocity = ConvertVectorFromSimulation(Particle.Velocity, EParticleValueSpace::World);
	CollisionEvent.Normal = CollisionNormal;
	CollisionEvent.ImpactVelocity = ImpactVelocityWorld;
	CollisionEvent.Item = Hit.FaceIndex;
	EmitCollisionEvent(CollisionEvent);
}

bool FParticleEmitterInstance::ResolveSingleParticleCollision(
	FBaseParticle& Particle,
	const UParticleModuleCollision& CollisionModule,
	uint32 ModuleOffset,
	float DeltaTime)
{
	if (FinalizeParticleCollisionWithoutQuery(Particle, CollisionModule, ModuleOffset))
	{
		return false;
	}

	auto* Payload =
		PARTICLE_PAYLOAD(&Particle, ModuleOffset, UParticleModuleCollision::FCollisionParticlePayload);
	if (!Payload)
	{
		return false;
	}

	const FVector StartWorld = ConvertPositionFromSimulation(Particle.OldLocation, EParticleValueSpace::World);
	const FVector EndWorld = ConvertPositionFromSimulation(Particle.Location, EParticleValueSpace::World);
	const FVector Travel = EndWorld - StartWorld;
	const float TravelDistance = Travel.Length();

	if (TravelDistance <= ParticleCollisionMinTravelDistance)
	{
		return false;
	}

	FVector TravelDirection = Travel / TravelDistance;
	if (TravelDirection.IsNearlyZero())
	{
		return false;
	}

	UWorld* World = Component ? Component->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	const AActor* IgnoreActor = Component ? Component->GetOwner() : nullptr;
	if (!World->PhysicsRaycast(
		StartWorld,
		TravelDirection,
		TravelDistance,
		Hit,
		CollisionModule.CollisionChannel,
		IgnoreActor))
	{
		return false;
	}

	const FVector ImpactVelocityWorld =
		ConvertVectorFromSimulation(Particle.Velocity, EParticleValueSpace::World);
	const float CollisionTimeSeconds = EmitterTimeSeconds + DeltaTime;
	FVector CollisionNormal = Hit.ImpactNormal;
	if (CollisionNormal.IsNearlyZero())
	{
		CollisionNormal = FVector::UpVector;
	}
	else
	{
		CollisionNormal.Normalize();
	}

	if (ShouldSuppressRepeatedCollisionHit(*Payload, CollisionNormal, CollisionTimeSeconds))
	{
		// This looks like repeated contact with the same surface within a very short
		// time window. Calm the contact without incrementing collision count or
		// emitting a normal collision event. This is "noise suppressed", not an
		// accepted collision report.
		const FVector AdjustedWorldPosition =
			Hit.WorldHitLocation + CollisionNormal * ParticleCollisionSurfaceOffset;
		const float NormalSpeed = ImpactVelocityWorld.Dot(CollisionNormal);
		FVector StabilizedVelocityWorld =
			ImpactVelocityWorld - CollisionNormal * std::min(NormalSpeed, 0.0f);

		if (!IsMeaningfulCollisionSpeed(GetCollisionSpeed(StabilizedVelocityWorld)))
		{
			StabilizedVelocityWorld = FVector::ZeroVector;
		}

		Particle.Location =
			ConvertPositionToSimulation(AdjustedWorldPosition, EParticleValueSpace::World);
		Particle.OldLocation = Particle.Location;
		Particle.Velocity =
			ConvertVectorToSimulation(StabilizedVelocityWorld, EParticleValueSpace::World);
		UpdateRecentCollisionState(*Payload, CollisionTimeSeconds, CollisionNormal);
		return false;
	}

	const bool bKillImmediately =
		ApplyImmediateParticleCollisionResponse(Particle, CollisionModule, Hit, ImpactVelocityWorld);
	UpdateRecentCollisionState(*Payload, CollisionTimeSeconds, CollisionNormal);

	// From this point on the hit is treated as an accepted / meaningful collision:
	// it passed raw hit detection, budget processing, and repeated-contact noise
	// suppression. Reporting remains a separate policy decision.
	++Payload->NumCollisions;

	if (ShouldEmitCollisionEventForAcceptedHit(CollisionModule))
	{
		// Base collision events represent accepted collision reports, not just any
		// raw raycast hit. They are emitted before final kill/freeze/ignore
		// completion consequences so response-mode timing stays predictable.
		EmitCollisionEventForAcceptedHit(
			Particle,
			CollisionNormal,
			ImpactVelocityWorld,
			Hit,
			CollisionTimeSeconds);
	}

	if (bKillImmediately)
	{
		Particle.Flags |= static_cast<uint32>(EParticleStateFlags::Killed);
		return true;
	}

	if (HasCollisionCountLimit(CollisionModule) && Payload->NumCollisions >= CollisionModule.MaxCollisions)
	{
		// Completion semantics are separate from the immediate response. A particle
		// may bounce/stop on this hit, then transition into Kill/Freeze/Ignore once
		// the configured collision count has been exhausted.
		ApplyCollisionCompletionBehavior(Particle, *Payload, CollisionModule);
	}

	return true;
}

bool FParticleEmitterInstance::ApplyImmediateParticleCollisionResponse(
	FBaseParticle& Particle,
	const UParticleModuleCollision& CollisionModule,
	const FHitResult& Hit,
	const FVector& ImpactVelocity) const
{
	FVector CollisionNormal = Hit.ImpactNormal;
	if (CollisionNormal.IsNearlyZero())
	{
		CollisionNormal = FVector::UpVector;
	}
	else
	{
		CollisionNormal.Normalize();
	}

	const FVector ReflectedVelocityWorld =
		ReflectVelocityAboutNormal(ImpactVelocity, CollisionNormal) *
		std::max(0.0f, CollisionModule.DampingFactor);

	const FVector AdjustedWorldPosition =
		Hit.WorldHitLocation + CollisionNormal * ParticleCollisionSurfaceOffset;

	const bool bLowSpeedImpact =
		!IsMeaningfulCollisionSpeed(GetCollisionSpeed(ImpactVelocity));

	// Immediate response answers "what happens on this hit?" Completion semantics
	// answer what to do only after the configured collision-count limit is reached.
	switch (ResolveImmediateCollisionResponseMode(CollisionModule))
	{
	case UParticleModuleCollision::ECollisionResponseMode::Kill:
		Particle.Location =
			ConvertPositionToSimulation(AdjustedWorldPosition, EParticleValueSpace::World);
		Particle.OldLocation = Particle.Location;
		Particle.Velocity = FVector::ZeroVector;
		return true;
	case UParticleModuleCollision::ECollisionResponseMode::Stop:
		Particle.Location =
			ConvertPositionToSimulation(AdjustedWorldPosition, EParticleValueSpace::World);
		Particle.OldLocation = Particle.Location;
		Particle.Velocity = FVector::ZeroVector;
		return false;
	case UParticleModuleCollision::ECollisionResponseMode::Bounce:
	default:
		Particle.Location =
			ConvertPositionToSimulation(AdjustedWorldPosition, EParticleValueSpace::World);
		Particle.OldLocation = Particle.Location;
		// Very low-speed contacts are usually repeated-contact noise rather than a
		// visually meaningful new bounce. Calm them into a stop-like response.
		Particle.Velocity = bLowSpeedImpact
			? FVector::ZeroVector
			: ConvertVectorToSimulation(ReflectedVelocityWorld, EParticleValueSpace::World);
		return false;
	}
}

bool FParticleEmitterInstance::ShouldProcessCollisionsForCurrentLOD() const
{
	return !IsCollisionFullyDisabledForCurrentLOD();
}

bool FParticleEmitterInstance::IsCollisionFullyDisabledForCurrentLOD() const
{
	// Full collision disable is the strongest LOD reduction axis. It is kept
	// separate from reduced budgets or event gating so those policies can evolve
	// independently.
	return CurrentLODIndex >= 2;
}

bool FParticleEmitterInstance::ShouldEmitCollisionEventsForCurrentLOD() const
{
	// Collision queries and collision-event fidelity are distinct reduction axes.
	// Lower LODs may keep some collision behavior while choosing not to preserve
	// gameplay/event-facing collision reporting.
	return CurrentLODIndex <= 0;
}

int32 FParticleEmitterInstance::GetCollisionCheckBudgetForCurrentLOD() const
{
	// Collision is now a lower-LOD runtime cost target. Budget scaling is one
	// reduction axis; full disable and event gating are handled separately.
	if (CurrentLODIndex <= 0)
	{
		return ParticleCollisionBudgetLOD0;
	}

	if (CurrentLODIndex == 1)
	{
		return ParticleCollisionBudgetLOD1;
	}

	return ParticleCollisionBudgetLOD2OrLower;
}

void FParticleEmitterInstance::ResizeParticleData(uint32 NewMax)
{
	NewMax = std::min<uint32>(NewMax, ParticleConstants::MaxParticlesPerEmitter);

	FParticleStorage OldStorage = std::move(RuntimeStorage);
	const uint32 OldActiveParticles = ActiveParticles;

	MaxActiveParticles = NewMax;
	const uint32 InstancePayloadBytes = Emitter ? Emitter->GetReqInstanceBytes() : 0;

	RuntimeStorage.Allocate(
		MaxActiveParticles * ParticleStride,
		MaxActiveParticles,
		InstancePayloadBytes);

	if (OldStorage.ParticleData && RuntimeStorage.ParticleData)
	{
		const uint32 PreservedActiveParticles = std::min(OldActiveParticles, MaxActiveParticles);
		const uint32 BytesToCopy = std::min(
			OldStorage.ParticleDataBytes,
			PreservedActiveParticles * ParticleStride);

		if (BytesToCopy > 0)
		{
			std::memcpy(RuntimeStorage.ParticleData, OldStorage.ParticleData, BytesToCopy);
		}
	}

	if (OldStorage.InstanceData && RuntimeStorage.InstanceData)
	{
		const uint32 InstanceBytesToCopy = std::min(
			OldStorage.InstanceDataBytes,
			RuntimeStorage.InstanceDataBytes);

		if (InstanceBytesToCopy > 0)
		{
			std::memcpy(RuntimeStorage.InstanceData, OldStorage.InstanceData, InstanceBytesToCopy);
		}
	}

	const uint32 PreservedIndexCount = std::min(
		ActiveParticles,
		std::min(OldStorage.ParticleIndexCount, RuntimeStorage.ParticleIndexCount));

	for (uint32 i = 0; i < PreservedIndexCount; ++i)
	{
		RuntimeStorage.ParticleIndices[i] = OldStorage.ParticleIndices[i];
	}

	for (uint32 i = PreservedIndexCount; i < MaxActiveParticles; ++i)
	{
		RuntimeStorage.ParticleIndices[i] = static_cast<uint16>(i);
	}

	if (ActiveParticles > MaxActiveParticles)
	{
		ActiveParticles = MaxActiveParticles;
	}
}

void FParticleEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData) const
{
	OutData.ActiveParticleCount = ActiveParticles;
	OutData.ParticleStride = ParticleStride;

	OutData.SnapshotStorage.Allocate(ActiveParticles * ParticleStride, ActiveParticles, 0);

	for (uint32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAt(i);
		if (!Particle) continue;

		std::memcpy(
			OutData.SnapshotStorage.ParticleData + i * ParticleStride,
			Particle,
			ParticleStride);

		OutData.SnapshotStorage.ParticleIndices[i] = static_cast<uint16>(i);
	}

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->RequiredModule)
	{
		return;
	}

	UParticleModuleRequired* Required = LOD->RequiredModule;

	OutData.Material = Required->ResolveMaterial();
	// SortMode는 material blend와 별개로 "어떤 기준으로 particle를 재배치할지"를 RT에 알려준다.
	OutData.SortMode = ToReplaySortMode(Required->SortMode);
	// NOTE: Replay에 BlendState 필드 없음 — Material.GetBlendState()가 single source of truth.
	// RequiredModule.BlendState로 Material을 override하고 싶으면 SceneProxy의
	// Material 캐싱 단계에서 SetBlendState 같은 API 추가 필요 (현재 RequiredModule.SubImagesH/V와 동일 패턴).
	OutData.bUseLocalSpace = Required->bUseLocalSpace;

	if (Component)
	{
		OutData.LocalToWorld = Component->GetWorldMatrix();
	}
}

uint32 FParticleEmitterInstance::GetInitialParticleCapacity() const
{
	return 64;
}

uint32 FParticleEmitterInstance::GrowParticleCapacity(uint32 Current, uint32 Required) const
{
	uint32 NewCapacity = Current > 0 ? Current : GetInitialParticleCapacity();

	while (NewCapacity < Required)
	{
		NewCapacity *= 2;
	}

	return std::min(NewCapacity, ParticleConstants::MaxParticlesPerEmitter);
}

bool FParticleEmitterInstance::IsParticleKilled(const FBaseParticle* Particle) const
{
	if (!Particle) return true;

	const uint32 KilledFlag = static_cast<uint32>(EParticleStateFlags::Killed);
	return (Particle->Flags & KilledFlag) != 0 || Particle->RelativeTime >= 1.0f;
}

void FParticleEmitterInstance::ClearSpawnedFlag(FBaseParticle* Particle) const
{
	if (!Particle) return;

	const uint32 SpawnedFlag = static_cast<uint32>(EParticleStateFlags::Spawned);
	Particle->Flags &= ~SpawnedFlag;
}

const UParticleModuleRequired* FParticleEmitterInstance::GetRequiredModule() const
{
	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD)
	{
		return nullptr;
	}

	return LOD->RequiredModule;
}

bool FParticleEmitterInstance::IsSpawningAllowed() const
{
	return !IsSpawningComplete();
}

void FParticleEmitterInstance::AdvanceLoopState(float DeltaTime)
{
	EmitterTimeSeconds += DeltaTime;

	const UParticleModuleRequired* Required = GetRequiredModule();
	if (!Required || Required->EmitterDuration <= 0.0f)
	{
		CurrentLoopTimeSeconds += DeltaTime;
		return;
	}

	CurrentLoopTimeSeconds += DeltaTime;

	const float Duration = Required->EmitterDuration;
	const float Epsilon = 1.0e-4f;
	// 큰 DeltaTime이 들어와 한 프레임 안에 여러 loop를 넘길 수 있으므로 while로 처리.
	while (CurrentLoopTimeSeconds >= Duration - Epsilon)
	{
		CurrentLoopTimeSeconds =
			(CurrentLoopTimeSeconds > Duration) ? (CurrentLoopTimeSeconds - Duration) : 0.0f;
		SpawnFraction = 0.0f;
		++LoopCount;

		UParticleLODLevel* LOD = GetCurrentLOD();
		if (LOD && LOD->SpawnModule)
		{
			if (auto* Payload =
				GetModuleInstancePayload<UParticleModuleSpawn::FSpawnModuleInstancePayload>(LOD->SpawnModule))
			{
				Payload->LastProcessedBurstTime = 0.0f;
			}
		}

		if (Duration <= Epsilon)
		{
			break;
		}
	}
}

// -- Sprite ----
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData()
{
	FDynamicSpriteEmitterData* Data = new FDynamicSpriteEmitterData();

	FillReplayData(Data->Source);
	Data->Source.EmitterType = EDynamicEmitterType::Sprite;

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (LOD && LOD->RequiredModule)
	{
		Data->Source.SubImagesHorizontal = LOD->RequiredModule->SubImagesHorizontal;
		Data->Source.SubImagesVertical = LOD->RequiredModule->SubImagesVertical;
		Data->Source.Alignment = ToReplaySpriteAlignment(LOD->RequiredModule->ScreenAlignment);
	}

	return Data;
}

// -- Mesh ----
FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData()
{
	FDynamicMeshEmitterData* Data = new FDynamicMeshEmitterData();
	FillReplayData(Data->Source);
	Data->Source.EmitterType = EDynamicEmitterType::Mesh;

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (LOD)
	{
		if (auto* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule))
		{
			Data->Source.Mesh = MeshTypeData->ResolveMesh();
			Data->Source.Alignment = ToReplayMeshAlignment(MeshTypeData->Alignment);
			Data->Source.bOverrideMaterial = MeshTypeData->bOverrideMaterial;
		}
	}

	// Mesh resolve가 실패해도 emitter type 자체는 Mesh로 유지한다.
	// RT는 nullptr mesh를 보고 기존 fallback mesh 경로를 그대로 사용할 수 있다.
	return Data;
}

// -- Beam ----
FDynamicEmitterDataBase* FParticleBeamEmitterInstance::GetDynamicData()
{
	FDynamicBeamEmitterData* Data = new FDynamicBeamEmitterData();

	FillReplayData(Data->Source);
	Data->Source.EmitterType = EDynamicEmitterType::Beam;

	Data->Source.SourcePoint = SourcePoint;
	Data->Source.TargetPoint = TargetPoint;

	if (UParticleLODLevel* LOD = GetCurrentLOD())
	{
		if (auto* BeamTypeData = Cast<UParticleModuleTypeDataBeam>(LOD->TypeDataModule))
		{
			Data->Source.InterpolationPoints = BeamTypeData->InterpolationPoints;
			Data->Source.Width = BeamTypeData->Width;
			Data->Source.NoiseAmount = BeamTypeData->NoiseAmount;
			Data->Source.NoiseFrequency = BeamTypeData->NoiseFrequency;
			Data->Source.NoiseSpeed = BeamTypeData->NoiseSpeed;
			Data->Source.bTileUV = BeamTypeData->bTileUV;
		}
		Data->Source.EmitterTime = EmitterTimeSeconds;
	}

	return Data;
}
void FParticleBeamEmitterInstance::SetEndpoints(const FVector& InSource, const FVector& InTarget)
{
	SourcePoint = InSource;
	TargetPoint = InTarget;
}

// -- Ribbon ----
FDynamicEmitterDataBase* FParticleRibbonEmitterInstance::GetDynamicData()
{
	FDynamicRibbonEmitterData* Data = new FDynamicRibbonEmitterData();
	FillReplayData(Data->Source);
	Data->Source.EmitterType = EDynamicEmitterType::Ribbon;

	if (UParticleLODLevel* LOD = GetCurrentLOD())
	{
		if (auto* RibbonTypeData = Cast<UParticleModuleTypeDataRibbon>(LOD->TypeDataModule))
		{
			Data->Source.MaxTessellation = RibbonTypeData->MaxTessellation;
			Data->Source.TangentTension = RibbonTypeData->TangentTension;
			Data->Source.TilesPerTrail = RibbonTypeData->TilesPerTrail;
		}
	}

	return Data;
}
