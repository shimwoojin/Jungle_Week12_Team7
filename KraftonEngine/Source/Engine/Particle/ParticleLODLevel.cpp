#include "ParticleLODLevel.h"

#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Serialization/Archive.h"

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
	// TODO: LOD 보간/복사
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

		// 초기 정책 - 같은 Category는 1개만 허용
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
