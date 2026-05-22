#include "ParticleLODLevel.h"

#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"

void UParticleLODLevel::UpdateFromLOD0(UParticleLODLevel* LOD0) {}

bool UParticleLODLevel::ValidateModules() const
{
	// TODO: RequiredModule, SpawnModule 존재 / TypeData 최대 1개 / 카테고리 중복 검사.
	return RequiredModule != nullptr && SpawnModule != nullptr;
}

bool UParticleLODLevel::AddModule(UParticleModule* InModule)
{
	if (!InModule) return false;
	// TODO: IsUnique() 모듈은 카테고리별 슬롯에 자리잡고, 일반은 Modules push.
	return false;
}

bool UParticleLODLevel::RemoveModule(UParticleModule* InModule)
{
	if (!InModule) return false;
	// TODO: 슬롯/배열 양쪽에서 제거.
	return false;
}
