#include "ParticleEmitter.h"

#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Object/Reflection/ObjectFactory.h"

void UParticleEmitter::InitializeDefaultLODLevel() {}

UParticleLODLevel* UParticleEmitter::CreateLODLevel(int32 InLevel)      { return nullptr; }
void               UParticleEmitter::RemoveLODLevel(int32 InLevel)      {}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 InLevel) const
{
	if (InLevel < 0 || InLevel >= static_cast<int32>(LODLevels.size())) return nullptr;
	return LODLevels[InLevel];
}

UParticleLODLevel* UParticleEmitter::GetCurrentLODLevel(int32 InCurrentLODIdx) const
{
	if (LODLevels.empty()) return nullptr;
	int32 Idx = InCurrentLODIdx;
	if (Idx < 0) Idx = 0;
	if (Idx >= static_cast<int32>(LODLevels.size())) Idx = static_cast<int32>(LODLevels.size()) - 1;
	return LODLevels[Idx];
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// TODO: LOD0 의 RequiredModule / Modules 를 순회하며 RequiredBytes 누적
	//       → ParticleSize, ModuleOffsetMap, RequiredBytesPerInstance 채움.
	ParticleSize             = sizeof(FBaseParticle);
	RequiredBytesPerInstance = 0;
	ModuleOffsetMap.clear();
}

uint32 UParticleEmitter::GetModuleOffset(const UParticleModule* M) const
{
	auto It = ModuleOffsetMap.find(M);
	if (It == ModuleOffsetMap.end()) return 0;
	return It->second;
}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	// TODO: LOD0.TypeDataModule 이 nullptr 이 아니면 그쪽 CreateInstance.
	//       아니면 FParticleSpriteEmitterInstance 폴백.
	return nullptr;
}
