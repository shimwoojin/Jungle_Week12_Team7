#include "ParticleEmitterInstance.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Math/Transform.h"

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	ParticleData.clear();
	ParticleIndices.clear();
}

void FParticleEmitterInstance::Init(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent)
{
	Emitter = InEmitter;
	Component = InComponent;

	CurrentLODIndex = 0;
	SpawnFraction = 0.0f;
	EmitterTimeSeconds = 0.0f;
	LoopCount = 0;
	ActiveParticles = 0;

	ClearPendingEvents();

	if (Emitter)
	{
		Emitter->InitializeDefaultLODLevel();
		Emitter->CacheEmitterModuleInfo();

		ModuleOffsetMap = &Emitter->GetModuleOffsetMap();

		ParticleStride = std::max<uint32>(Emitter->GetParticleSize(), sizeof(FBaseParticle));
	}
	else
	{
		ModuleOffsetMap = nullptr;
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

	SpawnParticles(DeltaTime);
	UpdateParticles(DeltaTime);

	EmitterTimeSeconds += DeltaTime;
}

void FParticleEmitterInstance::Reset()
{
	ActiveParticles = 0;
	SpawnFraction = 0.0f;
	EmitterTimeSeconds = 0.0f;
	LoopCount = 0;

	ClearPendingEvents();

	for (uint32 i = 0; i < MaxActiveParticles; ++i)
	{
		ParticleIndices[i] = static_cast<uint16>(i);
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
	const uint16 Slot = ParticleIndices[InActiveIndex];
	return reinterpret_cast<FBaseParticle*>(ParticleData.data() + Slot * ParticleStride);
}

const FBaseParticle* FParticleEmitterInstance::GetParticleAt(uint32 InActiveIndex) const
{
	if (InActiveIndex >= ActiveParticles) return nullptr;
	const uint16 Slot = ParticleIndices[InActiveIndex];
	return reinterpret_cast<const FBaseParticle*>(ParticleData.data() + Slot * ParticleStride);
}

UParticleLODLevel* FParticleEmitterInstance::GetCurrentLOD() const
{
	if (!Emitter) return nullptr;
	return Emitter->GetCurrentLODLevel(CurrentLODIndex);
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

void FParticleEmitterInstance::SpawnParticles(float DeltaTime)
{
	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->bEnabled) return;
	if (!LOD->SpawnModule) return;

	// 초당 생성되는 입자 개수
	float SpawnAmount = 0.0f;
	int32 BurstCount = 0;

	LOD->SpawnModule->GetSpawnAmount(DeltaTime, EmitterTimeSeconds,SpawnAmount,BurstCount);

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
		uint8* ParticleBytes = ParticleData.data() + Slot * ParticleStride;

		std::memset(ParticleBytes, 0, ParticleStride);

		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleBytes);
		*Particle = FBaseParticle{};

		Particle->Flags = ActiveFlag | SpawnedFlag;

		if (Component)
		{
			Particle->Location = Component->GetWorldLocation();
			Particle->OldLocation = Particle->Location;
		}

		ParticleIndices[ActiveParticles] = static_cast<uint16>(Slot);
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
		SpawnEvents.push_back(SpawnEvent);
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
			DeathEvents.push_back(DeathEvent);

			continue;
		}

		ClearSpawnedFlag(Particle);

		if (WriteIndex != ReadIndex)
		{
			FBaseParticle* WriteParticle =
				reinterpret_cast<FBaseParticle*>(ParticleData.data() + WriteIndex * ParticleStride);

			std::memmove(WriteParticle, Particle, ParticleStride);
		}

		ParticleIndices[WriteIndex] = static_cast<uint16>(WriteIndex);
		++WriteIndex;
	}

	ActiveParticles = WriteIndex;
}

void FParticleEmitterInstance::ResizeParticleData(uint32 NewMax)
{
	NewMax = std::min<uint32>(NewMax, ParticleConstants::MaxParticlesPerEmitter);

	MaxActiveParticles = NewMax;

	ParticleData.resize(MaxActiveParticles * ParticleStride, 0);
	ParticleIndices.resize(MaxActiveParticles, 0);

	for (uint32 i = 0; i < MaxActiveParticles; ++i)
	{
		ParticleIndices[i] = static_cast<uint16>(i);
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

	OutData.ParticleData.resize(ActiveParticles * ParticleStride, 0);
	OutData.ParticleIndices.resize(ActiveParticles, 0);

	for (uint32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAt(i);
		if (!Particle) continue;

		std::memcpy(
			OutData.ParticleData.data() + i * ParticleStride,
			Particle,
			ParticleStride);

		OutData.ParticleIndices[i] = static_cast<uint16>(i);
	}

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (!LOD || !LOD->RequiredModule)
	{
		return;
	}

	UParticleModuleRequired* Required = LOD->RequiredModule;

	OutData.Material = Required->ResolveMaterial();
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

// -- Sprite ----
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData()    
{ 
	FDynamicSpriteEmitterData* Data = new FDynamicSpriteEmitterData();

	FillReplayData(Data->Source);

	UParticleLODLevel* LOD = GetCurrentLOD();
	if (LOD && LOD->RequiredModule)
	{
		Data->Source.SubImagesHorizontal = LOD->RequiredModule->SubImagesHorizontal;
		Data->Source.SubImagesVertical = LOD->RequiredModule->SubImagesVertical;
	}

	return Data;
}

// -- Mesh ----
FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData() 
{ 
	FDynamicMeshEmitterData* Data = new FDynamicMeshEmitterData();
	FillReplayData(Data->Source);
	return Data;
}

// -- Beam ----
FDynamicEmitterDataBase* FParticleBeamEmitterInstance::GetDynamicData()     
{ 
	FDynamicBeamEmitterData* Data = new FDynamicBeamEmitterData();

	FillReplayData(Data->Source);

	Data->Source.SourcePoint = SourcePoint;
	Data->Source.TargetPoint = TargetPoint;

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
	return Data;
}
