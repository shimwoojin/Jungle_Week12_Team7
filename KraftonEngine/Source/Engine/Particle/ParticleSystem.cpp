#include "ParticleSystem.h"

#include "ParticleLODLevel.h"
#include "Particle/ParticleEmitter.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"

void UParticleSystem::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	// TODO: Emitters / 메타 직렬화
}

UObject* UParticleSystem::Duplicate(UObject* NewOuter) const
{
	// TODO: emitter / LOD / module 까지 deep duplicate
	return UObject::Duplicate(NewOuter);
}

UParticleEmitter* UParticleSystem::AddEmitter()      
{ 
	UParticleEmitter* NewEmitter = UObjectManager::Get().CreateObject<UParticleEmitter>(this);
	if (!NewEmitter) return nullptr;

	NewEmitter->EmitterName = MakeUniqueEmitterName();
	NewEmitter->InitializeDefaultLODLevel();

	Emitters.push_back(NewEmitter);
	return NewEmitter;
}

void UParticleSystem::RemoveEmitter(int32 Index)
{
	if (Index < 0 || Index >= static_cast<int32>(Emitters.size())) return;

	// TODO: 우선 참조 제거, 나중에 UObjectManager::DestroyObject까지 하면 LOD/Module 자식 정리 정책도 같이 잡아야 함
	Emitters.erase(Emitters.begin() + Index);
}

void UParticleSystem::MoveEmitter(int32 FromIndex, int32 ToIndex)
{
	const int32 Count = static_cast<int32>(Emitters.size());
	if (FromIndex < 0 || FromIndex >= Count) return;
	if (ToIndex < 0 || ToIndex >= Count) return;
	if (FromIndex == ToIndex) return;

	UParticleEmitter* Moving = Emitters[FromIndex];
	Emitters.erase(Emitters.begin() + FromIndex);
	Emitters.insert(Emitters.begin() + ToIndex, Moving);
}

UParticleEmitter* UParticleSystem::GetEmitter(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(Emitters.size())) return nullptr;
	return Emitters[Index];
}

void UParticleSystem::BuildEmitters()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter) continue;
		Emitter->InitializeDefaultLODLevel();

		UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);
		if (!LOD0 || !LOD0->ValidateModules())
		{
			continue;
		}

		Emitter->CacheEmitterModuleInfo();
	}
}

FString UParticleSystem::MakeUniqueEmitterName()
{
	int32 Index = 0;

	while (true)
	{
		FString Candidate = "Emitter " + std::to_string(Index);

		bool bExists = false;
		for (UParticleEmitter* Emitter : Emitters)
		{
			if (Emitter && Emitter->EmitterName == Candidate)
			{
				bExists = true;
				break;
			}
		}

		if (!bExists)
		{
			return Candidate;
		}

		++Index;
	}
}
