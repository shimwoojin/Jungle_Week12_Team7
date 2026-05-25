#include "ParticleLODLevel.h"

#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Serialization/Archive.h"

namespace
{
	using ELODModuleSyncMode = UParticleLODLevel::ELODModuleSyncMode;

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

	uint8 EncodeSyncMode(ELODModuleSyncMode InMode)
	{
		return static_cast<uint8>(InMode);
	}

	ELODModuleSyncMode DecodeSyncMode(uint8 RawMode)
	{
		return (RawMode == EncodeSyncMode(ELODModuleSyncMode::Override))
			? ELODModuleSyncMode::Override
			: ELODModuleSyncMode::InheritFromLOD0;
	}

	template<typename T>
	void SyncCoreModuleSlotFromLOD0(
		T*& TargetModule,
		T* SourceModule,
		UParticleLODLevel* TargetLOD,
		bool bShouldSyncFromLOD0)
	{
		if (!bShouldSyncFromLOD0)
		{
			return;
		}

		DestroyLODModule(TargetModule);
		TargetModule = DuplicateModuleForLOD(SourceModule, TargetLOD);
	}

	void FullCopyRegularModulesFromLOD0(UParticleLODLevel* TargetLOD, UParticleLODLevel* LOD0)
	{
		for (UParticleModule*& Module : TargetLOD->Modules)
		{
			DestroyLODModule(Module);
		}
		TargetLOD->Modules.clear();

		for (UParticleModule* SourceModule : LOD0->Modules)
		{
			UParticleModule* DuplicatedModule = DuplicateModuleForLOD(SourceModule, TargetLOD);
			if (!DuplicatedModule)
			{
				continue;
			}

			TargetLOD->Modules.push_back(DuplicatedModule);
		}

		TargetLOD->ResetRegularModuleSyncModes(ELODModuleSyncMode::InheritFromLOD0);
	}

	void SyncRegularModulesFromLOD0(UParticleLODLevel* TargetLOD, UParticleLODLevel* LOD0)
	{
		if (!TargetLOD->HasRegularModuleOverrides() ||
			TargetLOD->RegularModuleSyncModes.size() != TargetLOD->Modules.size())
		{
			// Full-copy remains the compatibility bridge while we introduce
			// module-level override metadata incrementally.
			FullCopyRegularModulesFromLOD0(TargetLOD, LOD0);
			return;
		}

		TArray<UParticleModule*> PreviousModules = TargetLOD->Modules;
		TArray<uint8> PreviousSyncModes = TargetLOD->RegularModuleSyncModes;
		TargetLOD->Modules.clear();
		TargetLOD->RegularModuleSyncModes.clear();

		for (int32 SourceIndex = 0; SourceIndex < static_cast<int32>(LOD0->Modules.size()); ++SourceIndex)
		{
			const bool bKeepOverrideModule =
				SourceIndex < static_cast<int32>(PreviousModules.size()) &&
				SourceIndex < static_cast<int32>(PreviousSyncModes.size()) &&
				DecodeSyncMode(PreviousSyncModes[SourceIndex]) == ELODModuleSyncMode::Override &&
				PreviousModules[SourceIndex] != nullptr;

			if (bKeepOverrideModule)
			{
				PreviousModules[SourceIndex]->SetOuter(TargetLOD);
				TargetLOD->Modules.push_back(PreviousModules[SourceIndex]);
				TargetLOD->SetRegularModuleSyncMode(
					static_cast<int32>(TargetLOD->Modules.size()) - 1,
					ELODModuleSyncMode::Override);
				PreviousModules[SourceIndex] = nullptr;
				continue;
			}

			UParticleModule* DuplicatedModule = DuplicateModuleForLOD(LOD0->Modules[SourceIndex], TargetLOD);
			if (!DuplicatedModule)
			{
				continue;
			}

			TargetLOD->Modules.push_back(DuplicatedModule);
			TargetLOD->SetRegularModuleSyncMode(
				static_cast<int32>(TargetLOD->Modules.size()) - 1,
				ELODModuleSyncMode::InheritFromLOD0);
		}

		for (int32 PreviousIndex = static_cast<int32>(LOD0->Modules.size());
		     PreviousIndex < static_cast<int32>(PreviousModules.size());
		     ++PreviousIndex)
		{
			const bool bKeepExtraOverrideModule =
				PreviousIndex < static_cast<int32>(PreviousSyncModes.size()) &&
				DecodeSyncMode(PreviousSyncModes[PreviousIndex]) == ELODModuleSyncMode::Override &&
				PreviousModules[PreviousIndex] != nullptr;

			if (!bKeepExtraOverrideModule)
			{
				continue;
			}

			PreviousModules[PreviousIndex]->SetOuter(TargetLOD);
			TargetLOD->Modules.push_back(PreviousModules[PreviousIndex]);
			TargetLOD->SetRegularModuleSyncMode(
				static_cast<int32>(TargetLOD->Modules.size()) - 1,
				ELODModuleSyncMode::Override);
			PreviousModules[PreviousIndex] = nullptr;
		}

		for (UParticleModule*& PreviousModule : PreviousModules)
		{
			DestroyLODModule(PreviousModule);
		}
	}

	void ApplyDeferredLODReductionPolicy(UParticleLODLevel* /*TargetLOD*/)
	{
		// Future LOD reduction/scaling policy should live here instead of being
		// mixed into structural sync. Likely candidates include spawn rate, beam
		// noise, ribbon tessellation, and collision/event disabling.
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

	if (RegularModuleSyncModes.size() != Modules.size())
	{
		ResetRegularModuleSyncModes();
	}
}

void UParticleLODLevel::UpdateFromLOD0(UParticleLODLevel* LOD0)
{
	if (!LOD0 || LOD0 == this)
	{
		return;
	}

	// Phase 4 starts separating structural sync from future reduction policy.
	// Full-copy remains the migration fallback, but module-level override
	// metadata can already keep selected derived modules/slots independent.
	bEnabled = LOD0->bEnabled;

	SyncCoreModuleSlotFromLOD0(RequiredModule, LOD0->RequiredModule, this, bSyncRequiredModuleFromLOD0);
	SyncCoreModuleSlotFromLOD0(SpawnModule, LOD0->SpawnModule, this, bSyncSpawnModuleFromLOD0);
	SyncCoreModuleSlotFromLOD0(TypeDataModule, LOD0->TypeDataModule, this, bSyncTypeDataModuleFromLOD0);
	SyncRegularModulesFromLOD0(this, LOD0);
	ApplyDeferredLODReductionPolicy(this);
}

UParticleLODLevel::ELODModuleSyncMode UParticleLODLevel::GetRegularModuleSyncMode(int32 ModuleIndex) const
{
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(RegularModuleSyncModes.size()))
	{
		return ELODModuleSyncMode::InheritFromLOD0;
	}

	return DecodeSyncMode(RegularModuleSyncModes[ModuleIndex]);
}

void UParticleLODLevel::SetRegularModuleSyncMode(int32 ModuleIndex, ELODModuleSyncMode InMode)
{
	if (ModuleIndex < 0)
	{
		return;
	}

	while (static_cast<int32>(RegularModuleSyncModes.size()) <= ModuleIndex)
	{
		RegularModuleSyncModes.push_back(EncodeSyncMode(ELODModuleSyncMode::InheritFromLOD0));
	}

	RegularModuleSyncModes[ModuleIndex] = EncodeSyncMode(InMode);
}

bool UParticleLODLevel::HasRegularModuleOverrides() const
{
	for (uint8 RawMode : RegularModuleSyncModes)
	{
		if (DecodeSyncMode(RawMode) == ELODModuleSyncMode::Override)
		{
			return true;
		}
	}

	return false;
}

void UParticleLODLevel::ResetRegularModuleSyncModes(ELODModuleSyncMode DefaultMode)
{
	RegularModuleSyncModes.clear();

	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
	{
		RegularModuleSyncModes.push_back(EncodeSyncMode(DefaultMode));
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
		bSyncRequiredModuleFromLOD0 = false;
		return true;
	}
	case UParticleModule::EModuleCategory::Spawn:
	{
		auto* Spawn = Cast<UParticleModuleSpawn>(InModule);
		if (!Spawn) return false;
		if (SpawnModule && SpawnModule != Spawn) return false;

		SpawnModule = Spawn;
		SpawnModule->SetOuter(this);
		bSyncSpawnModuleFromLOD0 = false;
		return true;
	}
	case UParticleModule::EModuleCategory::TypeData:
	{
		auto* TypeData = Cast<UParticleModuleTypeDataBase>(InModule);
		if (!TypeData) return false;
		if (TypeDataModule && TypeDataModule != TypeData) return false;

		TypeDataModule = TypeData;
		TypeDataModule->SetOuter(this);
		bSyncTypeDataModuleFromLOD0 = false;
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
	SetRegularModuleSyncMode(static_cast<int32>(Modules.size()) - 1, ELODModuleSyncMode::Override);
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
		bSyncTypeDataModuleFromLOD0 = false;
		return true;
	}

	for (int32 i = 0; i < static_cast<int32>(Modules.size()); ++i)
	{
		if (Modules[i] == InModule)
		{
			Modules.erase(Modules.begin() + i);
			if (i < static_cast<int32>(RegularModuleSyncModes.size()))
			{
				RegularModuleSyncModes.erase(RegularModuleSyncModes.begin() + i);
			}
			return true;
		}
	}

	return false;
}
