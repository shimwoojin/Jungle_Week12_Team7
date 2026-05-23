#include "ParticleModule.h"

#include "Serialization/Archive.h"

void UParticleModule::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	SerializeProperties(Ar, PF_Save);
}

void UParticleModule::PostDuplicate()
{
	UObject::PostDuplicate();
}

// UParticleModule 베이스는 인터페이스만. 서브클래스가 모든 동작을 구현.
