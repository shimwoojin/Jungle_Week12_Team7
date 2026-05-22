#include "ParticleEmitterInstance.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Math/Transform.h"

FParticleEmitterInstance::~FParticleEmitterInstance() {}

void FParticleEmitterInstance::Init(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent)
{
	Emitter   = InEmitter;
	Component = InComponent;
	// TODO: emitter 의 ParticleSize/ModuleOffsetMap 캐시 + 초기 capacity 할당.
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	// TODO: SpawnParticles → UpdateParticles → Kill → 이벤트 큐 정리
}

void FParticleEmitterInstance::Reset()
{
	ActiveParticles    = 0;
	SpawnFraction      = 0.0f;
	EmitterTimeSeconds = 0.0f;
	LoopCount          = 0;
	ClearPendingEvents();
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
	// TODO: Component 의 world transform 반환.
	return FTransform{};
}

void FParticleEmitterInstance::ClearPendingEvents()
{
	SpawnEvents.clear();
	DeathEvents.clear();
	CollisionEvents.clear();
	BurstEvents.clear();
}

void FParticleEmitterInstance::SpawnParticles(float DeltaTime)      {}
void FParticleEmitterInstance::SpawnInternal(int32 Count, float SpawnTimeBase) {}
void FParticleEmitterInstance::UpdateParticles(float DeltaTime)     {}

void FParticleEmitterInstance::ResizeParticleData(uint32 NewMax)
{
	MaxActiveParticles = NewMax;
	ParticleData.resize(NewMax * ParticleStride, 0);
	ParticleIndices.resize(NewMax, 0);
}

// -- Sprite ----
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData()    { return nullptr; }

// -- Mesh ----
FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData()      { return nullptr; }

// -- Beam ----
FDynamicEmitterDataBase* FParticleBeamEmitterInstance::GetDynamicData()      { return nullptr; }
void FParticleBeamEmitterInstance::SetEndpoints(const FVector& InSource, const FVector& InTarget)
{
	SourcePoint = InSource;
	TargetPoint = InTarget;
}

// -- Ribbon ----
FDynamicEmitterDataBase* FParticleRibbonEmitterInstance::GetDynamicData()    { return nullptr; }
