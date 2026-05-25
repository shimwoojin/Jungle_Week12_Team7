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
#include "Modules/ParticleModuleColorOverLife.h"
#include "Modules/ParticleModuleSize.h"
#include "Modules/ParticleModuleSizeByLife.h"
#include "Serialization/Archive.h"
#include "Particle/Distributions/DistributionVectorUniform.h"

#include <type_traits>

namespace
{
	UDistributionVectorUniform* SetVectorUniform(UDistributionVector*& Distribution, UObject* Outer, const FVector& Min, const FVector& Max)
	{
		if (Distribution)
		{
			UObjectManager::Get().DestroyObject(Distribution);
			Distribution = nullptr;
		}

		auto* NewDistribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Outer);
		if (NewDistribution)
		{
			NewDistribution->Min = Min;
			NewDistribution->Max = Max;
			Distribution = NewDistribution;
		}
		return NewDistribution;
	}
}

void UParticleEmitter::OnPostLoad(FArchive& /*Ar*/)
{
	EnsureLODCoreModules();
	CacheEmitterModuleInfo();
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

	EnsureLODCoreModules();
	CacheEmitterModuleInfo();
}

void UParticleEmitter::InitializeDefaultLODLevel()
{
	EnsureLODCoreModules();

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

	auto HasModuleClass = [LOD0](auto* ClassTag) -> bool
		{
			using TModule = std::remove_pointer_t<decltype(ClassTag)>;
			for (UParticleModule* Module : LOD0->Modules)
			{
				if (Cast<TModule>(Module))
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

			SetVectorUniform(Velocity->StartVelocityDistribution, Velocity, { -3.0f, -3.0f, 1.0f }, { 3.0f, 3.0f, 3.0f });

			LOD0->AddModule(Velocity);
		}
	}

	if (!HasModuleClass(static_cast<UParticleModuleColor*>(nullptr)))
	{
		auto* Color = UObjectManager::Get().CreateObject<UParticleModuleColor>(LOD0);
		if (Color)
		{
			Color->SetToSensibleDefaults(this);
			LOD0->AddModule(Color);
		}
	}

	if (!HasModuleClass(static_cast<UParticleModuleColorOverLife*>(nullptr)))
	{
		auto* ColorOverLife = UObjectManager::Get().CreateObject<UParticleModuleColorOverLife>(LOD0);
		if (ColorOverLife)
		{
			ColorOverLife->SetToSensibleDefaults(this);
			LOD0->AddModule(ColorOverLife);
		}
	}

	if (!HasModuleClass(static_cast<UParticleModuleSize*>(nullptr)))
	{
		auto* Size = UObjectManager::Get().CreateObject<UParticleModuleSize>(LOD0);
		if (Size)
		{
			Size->SetToSensibleDefaults(this);

			SetVectorUniform(Size->StartSizeDistribution, Size, { 0.5f, 0.5f, 1.0f }, { 1.0f, 1.0f, 1.0f });

			LOD0->AddModule(Size);
		}
	}
}

void UParticleEmitter::EnsureLODCoreModules()
{
	if (LODLevels.empty())
	{
		CreateLODLevel(0);
	}

	for (UParticleLODLevel* LOD : LODLevels)
	{
		if (!LOD)
		{
			continue;
		}

		if (!LOD->RequiredModule)
		{
			auto* Required = UObjectManager::Get().CreateObject<UParticleModuleRequired>(LOD);
			if (Required)
			{
				Required->SetToSensibleDefaults(this);
				LOD->RequiredModule = Required;
			}
		}

		if (!LOD->SpawnModule)
		{
			auto* Spawn = UObjectManager::Get().CreateObject<UParticleModuleSpawn>(LOD);
			if (Spawn)
			{
				LOD->SpawnModule = Spawn;
			}
		}
	}
}

void UParticleEmitter::EnsureLOD0CoreModules()
{
	EnsureLODCoreModules();
}

UParticleLODLevel* UParticleEmitter::CreateLODLevel(int32 InLevel)     
{ 
	if (InLevel < 0) InLevel = 0;
	const bool bIsDerivedLOD = InLevel > 0;

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

	if (bIsDerivedLOD)
	{
		if (UParticleLODLevel* LOD0 = GetLODLevel(0))
		{
			// Phase 1 keeps lower LOD creation simple: start from a full LOD0 copy
			// and defer any reduction/interpolation policy to later passes.
			NewLOD->UpdateFromLOD0(LOD0);
		}
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
	CachedLayout.InstanceModuleOffsets.clear();

	EnsureLODCoreModules();

	auto CacheModule = [this](UParticleLODLevel* LOD, UParticleModule* Module)
		{
			if (!LOD || !Module) return;

			if (CachedLayout.ModuleOffsets.find(Module) == CachedLayout.ModuleOffsets.end())
			{
				const uint32 Bytes = Module->RequiredBytes(LOD);
				if (Bytes > 0)
				{
					CachedLayout.ModuleOffsets[Module] = CachedLayout.ParticleStride;
					CachedLayout.ParticleStride =
						ParticleUtils::AlignParticleDataSize(CachedLayout.ParticleStride + Bytes);
				}
			}

			if (CachedLayout.InstanceModuleOffsets.find(Module) == CachedLayout.InstanceModuleOffsets.end())
			{
				const uint32 InstanceBytes = Module->RequiredBytesPerInstance();
				if (InstanceBytes > 0)
				{
					CachedLayout.InstancePayloadSize =
						ParticleUtils::AlignParticleDataSize(CachedLayout.InstancePayloadSize);
					CachedLayout.InstanceModuleOffsets[Module] = CachedLayout.InstancePayloadSize;
					CachedLayout.InstancePayloadSize =
						ParticleUtils::AlignParticleDataSize(CachedLayout.InstancePayloadSize + InstanceBytes);
				}
			}
		};

	for (UParticleLODLevel* LOD : LODLevels)
	{
		if (!LOD) continue;

		CacheModule(LOD, LOD->RequiredModule);
		CacheModule(LOD, LOD->SpawnModule);
		CacheModule(LOD, LOD->TypeDataModule);

		for (UParticleModule* Module : LOD->Modules)
		{
			CacheModule(LOD, Module);
		}
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
	// Instance subclass is chosen from TypeData. TypeData should usually be
	// consistent across LODs, but do not hard-code LOD 0 here: use the first
	// valid TypeData available in the emitter's LOD list.
	for (UParticleLODLevel* LOD : LODLevels)
	{
		if (!LOD || !LOD->TypeDataModule)
		{
			continue;
		}

		if (FParticleEmitterInstance* Inst = LOD->TypeDataModule->CreateInstance(InComponent))
		{
			return Inst;
		}
	}

	return new FParticleSpriteEmitterInstance();
}
