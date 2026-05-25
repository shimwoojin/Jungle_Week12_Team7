#include "ParticleLODLevel.h"

#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Serialization/Archive.h"

namespace
{
	template<typename T>
	T* DuplicateModuleForLOD(T* SourceModule, UParticleLODLevel* TargetLOD)
	{
		if (!SourceModule || !TargetLOD)
		{
			return nullptr;
		}

		T* DuplicatedModule = Cast<T>(SourceModule->Duplicate(TargetLOD));
		if (DuplicatedModule)
		{
			DuplicatedModule->SetOuter(TargetLOD);
		}

		return DuplicatedModule;
	}

	template<typename T>
	void DestroyLODModule(T*& Module)
	{
		if (!Module)
		{
			return;
		}

		UObjectManager::Get().DestroyObject(Module);
		Module = nullptr;
	}
}

void UParticleLODLevel::PostDuplicate()
{
	UObject::PostDuplicate();

	if (RequiredModule)
	{
		RequiredModule->SetOuter(this);
		RequiredModule->PostDuplicate();
	}

	if (SpawnModule)
	{
		SpawnModule->SetOuter(this);
		SpawnModule->PostDuplicate();
	}

	if (TypeDataModule)
	{
		TypeDataModule->SetOuter(this);
		TypeDataModule->PostDuplicate();
	}

	for (UParticleModule* Module : Modules)
	{
		if (!Module)
		{
			continue;
		}

		Module->SetOuter(this);
		Module->PostDuplicate();
	}
}

void UParticleLODLevel::UpdateFromLOD0(UParticleLODLevel* LOD0)
{
	if (!LOD0 || LOD0 == this)
	{
		return;
	}

	// Phase 1 uses a full-copy policy only. Lower-LOD reduction/interpolation
	// rules are deferred so derived LODs first become valid, non-empty copies.
	DestroyLODModule(RequiredModule);
	DestroyLODModule(SpawnModule);
	DestroyLODModule(TypeDataModule);

	for (UParticleModule*& Module : Modules)
	{
		DestroyLODModule(Module);
	}
	Modules.clear();

	bEnabled = LOD0->bEnabled;

	RequiredModule = DuplicateModuleForLOD(LOD0->RequiredModule, this);
	SpawnModule = DuplicateModuleForLOD(LOD0->SpawnModule, this);
	TypeDataModule = DuplicateModuleForLOD(LOD0->TypeDataModule, this);

	for (UParticleModule* SourceModule : LOD0->Modules)
	{
		UParticleModule* DuplicatedModule = DuplicateModuleForLOD(SourceModule, this);
		if (!DuplicatedModule)
		{
			continue;
		}

		Modules.push_back(DuplicatedModule);
	}
}

bool UParticleLODLevel::ValidateModules() const
{
	if (!RequiredModule) return false;
	if (!SpawnModule) return false;

	if (RequiredModule->GetCategory() != UParticleModule::EModuleCategory::Required) return false;
	if (SpawnModule->GetCategory() != UParticleModule::EModuleCategory::Spawn) return false;
	if (TypeDataModule && TypeDataModule->GetCategory() != UParticleModule::EModuleCategory::TypeData) return false;

	for (int32 i = 0; i < static_cast<int32>(Modules.size()); ++i)
	{
		UParticleModule* A = Modules[i];
		if (!A) return false;

		const auto Category = A->GetCategory();

		if (Category == UParticleModule::EModuleCategory::Required ||
			Category == UParticleModule::EModuleCategory::Spawn ||
			Category == UParticleModule::EModuleCategory::TypeData)
		{
			return false;
		}

		for (int32 j = i + 1; j < static_cast<int32>(Modules.size()); ++j)
		{
			UParticleModule* B = Modules[j];
			if (!B) return false;

			if (A == B) return false;
		}
	}

	return true;
}

bool UParticleLODLevel::AddModule(UParticleModule* InModule)
{
	if (!InModule) return false;

	switch (InModule->GetCategory())
	{
	case UParticleModule::EModuleCategory::Required:
	{
		auto* Required = Cast<UParticleModuleRequired>(InModule);
		if (!Required) return false;
		if (RequiredModule && RequiredModule != Required) return false;

		RequiredModule = Required;
		RequiredModule->SetOuter(this);
		return true;
	}
	case UParticleModule::EModuleCategory::Spawn:
	{
		auto* Spawn = Cast<UParticleModuleSpawn>(InModule);
		if (!Spawn) return false;
		if (SpawnModule && SpawnModule != Spawn) return false;

		SpawnModule = Spawn;
		SpawnModule->SetOuter(this);
		return true;
	}
	case UParticleModule::EModuleCategory::TypeData:
	{
		auto* TypeData = Cast<UParticleModuleTypeDataBase>(InModule);
		if (!TypeData) return false;
		if (TypeDataModule && TypeDataModule != TypeData) return false;

		TypeDataModule = TypeData;
		TypeDataModule->SetOuter(this);
		return true;
	}
		
	default :
		break;
	}

	for (UParticleModule* Existing : Modules)
	{
		if (Existing == InModule) return false;
	}

	InModule->SetOuter(this);
	Modules.push_back(InModule);
	return true;
}

bool UParticleLODLevel::RemoveModule(UParticleModule* InModule)
{
	if (!InModule) return false;

	if (RequiredModule == InModule)
	{
		return false;	// 삭제 불가
	}

	if (SpawnModule == InModule)
	{
		return false;	// 삭제 불가
	}

	if (TypeDataModule == InModule)
	{
		TypeDataModule = nullptr;
		return true;
	}

	for (int32 i = 0; i < static_cast<int32>(Modules.size()); ++i)
	{
		if (Modules[i] == InModule)
		{
			Modules.erase(Modules.begin() + i);
			return true;
		}
	}

	return false;
}
