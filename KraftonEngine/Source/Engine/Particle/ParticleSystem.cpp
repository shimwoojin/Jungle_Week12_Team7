#include "ParticleSystem.h"

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

UParticleEmitter* UParticleSystem::AddEmitter()       { return nullptr; }
void              UParticleSystem::RemoveEmitter(int32 Index) {}
void              UParticleSystem::MoveEmitter(int32 FromIndex, int32 ToIndex) {}

UParticleEmitter* UParticleSystem::GetEmitter(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(Emitters.size())) return nullptr;
	return Emitters[Index];
}

void UParticleSystem::BuildEmitters() {}
