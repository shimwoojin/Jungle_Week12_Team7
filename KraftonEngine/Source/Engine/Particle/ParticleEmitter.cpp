#include "ParticleEmitter.h"

#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleSpawn.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Modules/ParticleModuleLifetime.h"
#include "Modules/ParticleModuleLocation.h"
#include "Modules/ParticleModuleVelocity.h"
#include "Modules/ParticleModuleColor.h"
#include "Modules/ParticleModuleSize.h"
#include "Serialization/Archive.h"

void UParticleEmitter::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	SerializeProperties(Ar, PF_Save);

	if (Ar.IsLoading())
	{
		EnsureLOD0CoreModules();
		CacheEmitterModuleInfo();
	}
}

void UParticleEmitter::PostDuplicate()
{
	UObject::PostDuplicate();

	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (!LODLevel)
		{
			continue;
		}

		LODLevel->SetOuter(this);
		LODLevel->PostDuplicate();
	}

	EnsureLOD0CoreModules();
	CacheEmitterModuleInfo();
}

void UParticleEmitter::InitializeDefaultLODLevel()
{
	EnsureLOD0CoreModules();

	UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (!LOD0)
	{
		return;
	}

	auto HasModuleCategory = [LOD0](UParticleModule::EModuleCategory Category) -> bool
		{
			for (UParticleModule* Module : LOD0->Modules)
			{
				if (Module && Module->GetCategory() == Category)
				{
					return true;
				}
			}
			return false;
		};

	if (!HasModuleCategory(UParticleModule::EModuleCategory::Lifetime))
	{
		auto* Lifetime = UObjectManager::Get().CreateObject<UParticleModuleLifetime>(LOD0);
		if (Lifetime)
		{
			Lifetime->SetToSensibleDefaults(this);
			LOD0->AddModule(Lifetime);
		}
	}

	if (!HasModuleCategory(UParticleModule::EModuleCategory::Location))
	{
		auto* Location = UObjectManager::Get().CreateObject<UParticleModuleLocation>(LOD0);
		if (Location)
		{
			Location->SetToSensibleDefaults(this);
			LOD0->AddModule(Location);
		}
	}

	if (!HasModuleCategory(UParticleModule::EModuleCategory::Velocity))
	{
		auto* Velocity = UObjectManager::Get().CreateObject<UParticleModuleVelocity>(LOD0);
		if (Velocity)
		{
			Velocity->SetToSensibleDefaults(this);

			// TODO: 추후 삭제 - 테스트용 기본값
			Velocity->StartVelocityMin = { 0.0f, 0.0f, 30.0f };
			Velocity->StartVelocityMax = { 0.0f, 0.0f, 80.0f };

			LOD0->AddModule(Velocity);
		}
	}

	if (!HasModuleCategory(UParticleModule::EModuleCategory::Color))
	{
		auto* Color = UObjectManager::Get().CreateObject<UParticleModuleColor>(LOD0);
		if (Color)
		{
			Color->SetToSensibleDefaults(this);
			LOD0->AddModule(Color);
		}
	}

	if (!HasModuleCategory(UParticleModule::EModuleCategory::Size))
	{
		auto* Size = UObjectManager::Get().CreateObject<UParticleModuleSize>(LOD0);
		if (Size)
		{
			Size->SetToSensibleDefaults(this);

			// TODO: 추후 삭제 - 테스트용 기본값
			Size->StartSizeMin = { 5.0f, 5.0f, 1.0f };
			Size->StartSizeMax = { 10.0f, 10.0f, 1.0f };

			LOD0->AddModule(Size);
		}
	}
}

void UParticleEmitter::EnsureLOD0CoreModules()
{
	UParticleLODLevel* LOD0 = nullptr;

	if (LODLevels.empty())
	{
		LOD0 = CreateLODLevel(0);
	}
	else
	{
		LOD0 = LODLevels[0];
	}

	if (!LOD0)
	{
		return;
	}

	if (!LOD0->RequiredModule)
	{
		auto* Required = UObjectManager::Get().CreateObject<UParticleModuleRequired>(LOD0);
		if (Required)
		{
			Required->SetToSensibleDefaults(this);
			LOD0->RequiredModule = Required;
		}
	}

	if (!LOD0->SpawnModule)
	{
		auto* Spawn = UObjectManager::Get().CreateObject<UParticleModuleSpawn>(LOD0);
		if (Spawn)
		{
			LOD0->SpawnModule = Spawn;
		}
	}
}

UParticleLODLevel* UParticleEmitter::CreateLODLevel(int32 InLevel)     
{ 
	if (InLevel < 0) InLevel = 0;

	if (UParticleLODLevel* Existing = GetLODLevel(InLevel))
	{
		return Existing;
	}

	UParticleLODLevel* NewLOD = UObjectManager::Get().CreateObject<UParticleLODLevel>(this);
	if (!NewLOD) return nullptr;

	NewLOD->Level = InLevel;
	NewLOD->bEnabled = true;

	auto* Required = UObjectManager::Get().CreateObject<UParticleModuleRequired>(NewLOD);
	Required->SetToSensibleDefaults(this);
	NewLOD->RequiredModule = Required;

	auto* Spawn = UObjectManager::Get().CreateObject<UParticleModuleSpawn>(NewLOD);
	NewLOD->SpawnModule = Spawn;

	if (InLevel >= static_cast<int32>(LODLevels.size()))
	{
		LODLevels.push_back(NewLOD);
	}
	else
	{
		LODLevels.insert(LODLevels.begin() + InLevel, NewLOD);
	}

	return NewLOD;
}

void  UParticleEmitter::RemoveLODLevel(int32 InLevel)
{
	if (InLevel < 0 || InLevel >= static_cast<int32>(LODLevels.size())) return;

	LODLevels.erase(LODLevels.begin() + InLevel);

	for (int32 i = 0; i < static_cast<int32>(LODLevels.size()); ++i)
	{
		if (LODLevels[i])
		{
			LODLevels[i]->Level = i;
		}
	}
}

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
	CachedLayout.ParticleStride = ParticleUtils::AlignParticleDataSize(sizeof(FBaseParticle));
	CachedLayout.InstancePayloadSize = 0;
	CachedLayout.ModuleOffsets.clear();

	UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (!LOD0) return;

	auto CacheModule = [this, LOD0](UParticleModule* Module)
		{
			if (!Module) return;

			const uint32 Bytes = Module->RequiredBytes(LOD0);
			if (Bytes > 0)
			{
				CachedLayout.ModuleOffsets[Module] = CachedLayout.ParticleStride;
				CachedLayout.ParticleStride =
					ParticleUtils::AlignParticleDataSize(CachedLayout.ParticleStride + Bytes);
			}

			CachedLayout.InstancePayloadSize += Module->RequiredBytesPerInstance();
		};

	CacheModule(LOD0->RequiredModule);
	CacheModule(LOD0->SpawnModule);
	CacheModule(LOD0->TypeDataModule);

	for (UParticleModule* Module : LOD0->Modules)
	{
		CacheModule(Module);
	}
}

uint32 UParticleEmitter::GetModuleOffset(const UParticleModule* M) const
{
	auto It = CachedLayout.ModuleOffsets.find(M);
	if (It == CachedLayout.ModuleOffsets.end()) return 0;
	return It->second;
}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	UParticleLODLevel* LOD0 = GetLODLevel(0);

	if (LOD0 && LOD0->TypeDataModule)
	{
		if (FParticleEmitterInstance* Inst = LOD0->TypeDataModule->CreateInstance(InComponent))
		{
			return Inst;
		}
	}

	return new FParticleSpriteEmitterInstance();
}
