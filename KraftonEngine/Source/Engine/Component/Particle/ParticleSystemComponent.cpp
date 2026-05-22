#include "ParticleSystemComponent.h"

#include "Particle/ParticleSystem.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEventManager.h"
#include "Render/Proxy/Particle/ParticleSystemSceneProxy.h"

UParticleSystemComponent::UParticleSystemComponent()  {}
UParticleSystemComponent::~UParticleSystemComponent() {}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	Template = InTemplate;
	// TODO: emitter instance 재생성 + render state dirty
}

void UParticleSystemComponent::Activate(bool bReset)
{
	bActive = true;
	if (bReset) ResetParticles();
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
}

void UParticleSystemComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();
	if (bAutoActivate) Activate(bResetOnActivate);
}

void UParticleSystemComponent::EndPlay()
{
	Deactivate();
	UPrimitiveComponent::EndPlay();
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// TODO: !bActive 면 skip
	//       TickInterval 적용 — Accumulator >= Interval 일 때만 tick
	//       모든 EmitterInstance->Tick(DeltaTime)
	//       PendingEvents merge → DispatchEventsToManager
	//       BuildDynamicData → SceneProxy 에 전달 (MarkProxyDirty)
}

void UParticleSystemComponent::CreateRenderState()
{
	UPrimitiveComponent::CreateRenderState();
	// TODO: CreateEmitterInstances + SceneProxy 생성.
}

void UParticleSystemComponent::DestroyRenderState()
{
	DestroyEmitterInstances();
	UPrimitiveComponent::DestroyRenderState();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName) {}
void UParticleSystemComponent::PostDuplicate() {}

void UParticleSystemComponent::UpdateWorldAABB() const
{
	// TODO: Template->SystemBounds + WorldTransform 으로 AABB 계산.
}

FParticleEmitterInstance* UParticleSystemComponent::GetEmitterInstance(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(EmitterInstances.size())) return nullptr;
	return EmitterInstances[Index];
}

UParticleSystemComponent::FDynamicData* UParticleSystemComponent::BuildDynamicData()
{
	// TODO: 각 EmitterInstance::GetDynamicData() 호출 → FDynamicData::Emitters 에 push.
	return nullptr;
}

void UParticleSystemComponent::CreateEmitterInstances() {}
void UParticleSystemComponent::DestroyEmitterInstances()
{
	for (FParticleEmitterInstance* Inst : EmitterInstances) delete Inst;
	EmitterInstances.clear();
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
