#include "ParticleSystemComponent.h"

#include "Particle/ParticleSystem.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEventManager.h"
#include "Render/Proxy/Particle/ParticleSystemSceneProxy.h"
#include "Serialization/Archive.h"

#include <algorithm>

UParticleSystemComponent::UParticleSystemComponent()  {}
UParticleSystemComponent::~UParticleSystemComponent()
{
	DestroyEmitterInstances();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	DestroyEmitterInstances();

	Template = InTemplate;
	AccumulatedTime = 0.0f;
	PendingEvents = {};

	if (Template)
	{
		Template->BuildEmitters();
		CreateEmitterInstances();
	}

	if (bResetOnActivate)
	{
		ResetParticles();
	}

	PushDynamicDataToProxy();
	MarkWorldBoundsDirty();
}

void UParticleSystemComponent::Activate(bool bReset)
{
	bActive = true;

	if (Template && EmitterInstances.empty())
	{
		Template->BuildEmitters();
		CreateEmitterInstances();
	}

	if (bReset || bResetOnActivate)
	{
		ResetParticles();
	}

	PushDynamicDataToProxy();
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UParticleSystemComponent::Deactivate()
{
	bActive = false;
}

void UParticleSystemComponent::ResetParticles()
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst) Inst->Reset();
	}
	AccumulatedTime = 0.0f;
	PendingEvents = {};

	PushDynamicDataToProxy();
	MarkWorldBoundsDirty();
}

void UParticleSystemComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();

	if (Template && EmitterInstances.empty())
	{
		Template->BuildEmitters();
	}

	if (bAutoActivate) Activate(bResetOnActivate);
}

void UParticleSystemComponent::EndPlay()
{
	Deactivate();
	DestroyEmitterInstances();

	UPrimitiveComponent::EndPlay();
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bActive) return;
	if (!Template) return;

	if (EmitterInstances.empty())
	{
		Template->BuildEmitters();
		CreateEmitterInstances();
	}

	ApplyCurrentLODToEmitterInstances();

	// 매 프레임 Tick이 아닌, 일정 간격마다 몰아서 Tick
	// 실제로 시뮬레이션에 넘길 DeltaTime
	float StepDeltaTime = DeltaTime;

	if (TickInterval > 0.0f)
	{
		AccumulatedTime += DeltaTime;

		if (AccumulatedTime < TickInterval)
		{
			return;
		}

		StepDeltaTime = AccumulatedTime;
		AccumulatedTime = 0.0f;
	}

	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (!Inst) continue;

		Inst->Tick(StepDeltaTime);

		const auto& SpawnEvents = Inst->GetSpawnEvents();
		PendingEvents.Spawn.insert(PendingEvents.Spawn.end(), SpawnEvents.begin(), SpawnEvents.end());

		const auto& DeathEvents = Inst->GetDeathEvents();
		PendingEvents.Death.insert(PendingEvents.Death.end(), DeathEvents.begin(), DeathEvents.end());

		const auto& CollisionEvents = Inst->GetCollisionEvents();
		PendingEvents.Collision.insert(PendingEvents.Collision.end(), CollisionEvents.begin(), CollisionEvents.end());

		const auto& BurstEvents = Inst->GetBurstEvents();
		PendingEvents.Burst.insert(PendingEvents.Burst.end(), BurstEvents.begin(), BurstEvents.end());

		Inst->ClearPendingEvents();
	}

	DispatchEventsToManager();

	PushDynamicDataToProxy();
	MarkWorldBoundsDirty();

	if (IsSystemFinished())
	{
		bActive = false;
		OnSystemFinished.Broadcast(this);
	}
}

void UParticleSystemComponent::CreateRenderState()
{
	if (Template && EmitterInstances.empty())
	{
		Template->BuildEmitters();
		CreateEmitterInstances();
	}
	UPrimitiveComponent::CreateRenderState();

	PushDynamicDataToProxy();
}

void UParticleSystemComponent::DestroyRenderState()
{
	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		FParticleSystemSceneProxy* ParticleProxy = static_cast<FParticleSystemSceneProxy*>(Proxy);
		ParticleProxy->SetDynamicData(nullptr);
	}

	UPrimitiveComponent::DestroyRenderState();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (!PropertyName) return;

	if (std::strcmp(PropertyName, "Template") == 0)
	{
		SetTemplate(Template);
		return;
	}

	if (std::strcmp(PropertyName, "CurrentLODIndex") == 0 ||
		std::strcmp(PropertyName, "LOD Level") == 0)
	{
		if (CurrentLODIndex < 0)
		{
			CurrentLODIndex = 0;
		}

		ApplyCurrentLODToEmitterInstances();

		PushDynamicDataToProxy();
		MarkWorldBoundsDirty();
		return;
	}

	if (std::strcmp(PropertyName, "TickInterval") == 0 || 
		std::strcmp(PropertyName, "Tick Interval (sec)") == 0)
	{
		if (TickInterval < 0.0f) TickInterval = 0.0f;
		AccumulatedTime = 0.0f;
		return;
	}

	if (std::strcmp(PropertyName, "bAutoActivate") == 0 ||
		std::strcmp(PropertyName, "Auto Activate") == 0)
	{
		// AutoActivate는 보통 BeginPlay/Create 시점 정책이라
		// 편집 즉시 Activate/Deactivate하지 않는 게 안전함.
		return;
	}

	if (std::strcmp(PropertyName, "bResetOnActivate") == 0 ||
		std::strcmp(PropertyName, "Reset On Activate") == 0)
	{
		// 단순 정책값. 즉시 재생성 불필요.
		return;
	}
}

void UParticleSystemComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	DestroyEmitterInstances();
	AccumulatedTime = 0.0f;
	PendingEvents = {};

	if (CurrentLODIndex < 0)
	{
		CurrentLODIndex = 0;
	}

	if (Template)
	{
		Template->BuildEmitters();
	}

	if (bAutoActivate)
	{
		Activate(true);
	}
	else
	{
		PushDynamicDataToProxy();
		MarkWorldBoundsDirty();
	}
}

void UParticleSystemComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		DestroyEmitterInstances();
		AccumulatedTime = 0.0f;
		PendingEvents = {};

		if (CurrentLODIndex < 0)
		{
			CurrentLODIndex = 0;
		}

		if (Template)
		{
			Template->BuildEmitters();
		}

		MarkWorldBoundsDirty();
	}
}

void UParticleSystemComponent::UpdateWorldAABB() const
{
	if (!Template)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	const FVector WorldOrigin = GetWorldLocation();
	bool bHasDynamicBounds = false;

	if (!Template->bUseFixedRelativeBoundingBox)
	{
		for (const FParticleEmitterInstance* Inst : EmitterInstances)
		{
			if (!Inst)
			{
				continue;
			}

			FVector EmitterBoundsMin;
			FVector EmitterBoundsMax;
			if (!Inst->ComputeDynamicBounds(EmitterBoundsMin, EmitterBoundsMax))
			{
				continue;
			}

			if (!bHasDynamicBounds)
			{
				WorldAABBMinLocation = EmitterBoundsMin;
				WorldAABBMaxLocation = EmitterBoundsMax;
				bHasDynamicBounds = true;
				continue;
			}

			WorldAABBMinLocation.X = std::min(WorldAABBMinLocation.X, EmitterBoundsMin.X);
			WorldAABBMinLocation.Y = std::min(WorldAABBMinLocation.Y, EmitterBoundsMin.Y);
			WorldAABBMinLocation.Z = std::min(WorldAABBMinLocation.Z, EmitterBoundsMin.Z);
			WorldAABBMaxLocation.X = std::max(WorldAABBMaxLocation.X, EmitterBoundsMax.X);
			WorldAABBMaxLocation.Y = std::max(WorldAABBMaxLocation.Y, EmitterBoundsMax.Y);
			WorldAABBMaxLocation.Z = std::max(WorldAABBMaxLocation.Z, EmitterBoundsMax.Z);
		}
	}

	if (!bHasDynamicBounds)
	{
		WorldAABBMinLocation = WorldOrigin + Template->SystemBoundsMin;
		WorldAABBMaxLocation = WorldOrigin + Template->SystemBoundsMax;
	}

	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

FParticleEmitterInstance* UParticleSystemComponent::GetEmitterInstance(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(EmitterInstances.size())) return nullptr;
	return EmitterInstances[Index];
}

void UParticleSystemComponent::RebuildInstances(bool bReset)
{
	if (Template)
	{
		Template->BuildEmitters();
		CreateEmitterInstances();
	}
	else
	{
		DestroyEmitterInstances();
	}

	if (bReset)
	{
		ResetParticles();
	}

	PushDynamicDataToProxy();
	MarkWorldBoundsDirty();
}

UParticleSystemComponent::FDynamicData* UParticleSystemComponent::BuildDynamicData()
{
	FDynamicData* Data = new FDynamicData();

	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (!Inst) continue;

		FDynamicEmitterDataBase* EmitterData = Inst->GetDynamicData();
		if (!EmitterData) continue;

		Data->Emitters.push_back(EmitterData);
	}

	return Data;
}

void UParticleSystemComponent::CreateEmitterInstances()
{
	DestroyEmitterInstances();

	if (!Template) return;

	Template->BuildEmitters();

	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (!Emitter) continue;
		if (!Emitter->bEnabled) continue;

		FParticleEmitterInstance* Inst = Emitter->CreateInstance(this);
		if (!Inst) continue;

		Inst->Init(Emitter, this);
		Inst->SetCurrentLODIndex(CurrentLODIndex);
		EmitterInstances.push_back(Inst);
	}
}
void UParticleSystemComponent::DestroyEmitterInstances()
{
	for (FParticleEmitterInstance* Inst : EmitterInstances) delete Inst;
	EmitterInstances.clear();
	PendingEvents = {};
}

void UParticleSystemComponent::DispatchEventsToManager()
{
	if (!EventManager) { PendingEvents = {}; return; }
	EventManager->HandleParticleSpawnEvents    (this, PendingEvents.Spawn);
	EventManager->HandleParticleDeathEvents    (this, PendingEvents.Death);
	EventManager->HandleParticleCollisionEvents(this, PendingEvents.Collision);
	EventManager->HandleParticleBurstEvents    (this, PendingEvents.Burst);
	PendingEvents = {};
}

void UParticleSystemComponent::ApplyCurrentLODToEmitterInstances()
{
	if (CurrentLODIndex < 0)
	{
		CurrentLODIndex = 0;
	}

	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (!Inst)
		{
			continue;
		}

		Inst->SetCurrentLODIndex(CurrentLODIndex);
	}
}

bool UParticleSystemComponent::IsSystemFinished() const
{
	if (EmitterInstances.empty())
	{
		return false;
	}

	for (const FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (!Inst || !Inst->IsFinished())
		{
			return false;
		}
	}

	return true;
}

void UParticleSystemComponent::PushDynamicDataToProxy()
{
	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		FParticleSystemSceneProxy* ParticleProxy = static_cast<FParticleSystemSceneProxy*>(Proxy);
		ParticleProxy->SetDynamicData(BuildDynamicData());
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}
