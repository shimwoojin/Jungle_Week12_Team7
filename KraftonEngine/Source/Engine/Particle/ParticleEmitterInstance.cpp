#include "ParticleEmitterInstance.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleEventGenerator.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam.h"
#include "Particle/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Math/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

	// 초당 생성되는 입자 개수
	float SpawnAmount = 0.0f;
	int32 BurstCount = 0;

	// SpawnModule은 "현재 loop 안에서의 상대 시간"만 알면 burst trigger를 계산할 수 있다.
	LOD->SpawnModule->GetSpawnAmount(this, DeltaTime, CurrentLoopTimeSeconds, SpawnAmount, BurstCount);

	SpawnAmount = std::max(0.0f, SpawnAmount);
	BurstCount = std::max(0, BurstCount);

	// Fraction - 이전 프레임에서 계산된 찌꺼기값
	const float TotalSpawnFloat = SpawnFraction + SpawnAmount;
	const int32 RateSpawnCount = static_cast<int32>(std::floor(TotalSpawnFloat));

	SpawnFraction = TotalSpawnFloat - static_cast<float>(RateSpawnCount);

	int32 SpawnCount = RateSpawnCount + BurstCount;
	if (SpawnCount <= 0) return;

	if (SpawnCount > static_cast<int32>(ParticleConstants::MaxBurstCountPerFrame))
	{
		SpawnCount = static_cast<int32>(ParticleConstants::MaxBurstCountPerFrame);
	}

	// 생성가능한 빈 자리 수
	const uint32 FreeCount =
		ParticleConstants::MaxParticlesPerEmitter > ActiveParticles
		? ParticleConstants::MaxParticlesPerEmitter - ActiveParticles
		: 0;

	if (FreeCount == 0) return;

	if (static_cast<uint32>(SpawnCount) > FreeCount)
	{
		SpawnCount = static_cast<int32>(FreeCount);
	}

	const int32 ActualBurstCount = std::min(BurstCount, SpawnCount);
	if (ActualBurstCount > 0)
	{
		FParticleEventBurstData BurstEvent;
		BurstEvent.Type = EParticleEventType::Burst;
		BurstEvent.TimeSeconds = EmitterTimeSeconds;
		BurstEvent.ParticleCount = ActualBurstCount;

		if (Component)
		{
			BurstEvent.Location = Component->GetWorldLocation();
		}

		EmitBurstEvent(BurstEvent);
	}

	// Spawn 진행
	SpawnInternal(SpawnCount, 0.0f);
}

void FParticleEmitterInstance::SpawnInternal(int32 Count, float SpawnTimeBase)
{
	if (Count <= 0) return;

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->bEnabled) return;

	const uint32 RequiredActiveCount = ActiveParticles + static_cast<uint32>(Count);
	if (RequiredActiveCount > MaxActiveParticles)
	{
		const uint32 NewCapacity = GrowParticleCapacity(MaxActiveParticles, RequiredActiveCount);
		if (NewCapacity <= MaxActiveParticles) return;

		ResizeParticleData(NewCapacity);
	}

	const uint32 ActiveFlag = static_cast<uint32>(EParticleStateFlags::Active);
	const uint32 SpawnedFlag = static_cast<uint32>(EParticleStateFlags::Spawned);

	for (int32 SpawnIndex = 0; SpawnIndex < Count; ++SpawnIndex)
	{
		if (ActiveParticles >= MaxActiveParticles) break;

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

		for (UParticleModule* Module : LOD->Modules)
		{
			if (!Module || !Module->IsEnabled()) continue;

			const uint32 ModuleOffset = GetModuleDataOffset(Module);
			Module->Spawn(this, ModuleOffset, SpawnTimeBase, Particle);
		}

		FParticleEventSpawnData SpawnEvent;
		SpawnEvent.Type = EParticleEventType::Spawn;
		SpawnEvent.TimeSeconds = EmitterTimeSeconds;
		SpawnEvent.Location = Particle->Location;
		SpawnEvent.Velocity = Particle->Velocity;
		SpawnEvent.ParticleCount = 1;
		EmitSpawnEvent(SpawnEvent);
	}
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
				Payload->LastProcessedTime = 0.0f;
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
		}
	}

	return Data;
}
