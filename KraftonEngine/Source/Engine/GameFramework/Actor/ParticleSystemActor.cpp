#include "ParticleSystemActor.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Particle/ParticleSystem.h"

void AParticleSystemActor::InitDefaultComponents()
{
	ParticleSystemComponent = AddComponent<UParticleSystemComponent>();
	SetRootComponent(ParticleSystemComponent);

	UParticleSystem* PS = UObjectManager::Get().CreateObject<UParticleSystem>();
	UParticleEmitter* Emitter = PS->AddEmitter();

	UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);
	assert(LOD0);
	assert(LOD0->RequiredModule);
	assert(LOD0->SpawnModule);
	assert(!LOD0->Modules.empty());

	ParticleSystemComponent->SetTemplate(PS);
	ParticleSystemComponent->Activate(true);
}

void AParticleSystemActor::PostDuplicate()
{
	AActor::PostDuplicate();
	ParticleSystemComponent = Cast<UParticleSystemComponent>(GetRootComponent());
}

void AParticleSystemActor::BeginPlay()
{
	AActor::BeginPlay();
}
