#include "ParticleSystemManager.h"

#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

#include "Core/Logging/Log.h"

UParticleSystem* FParticleSystemManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedParticleSystems.find(NormalizedPath);
	if (It != LoadedParticleSystems.end())
	{
		return It->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		return nullptr;
	}

	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid())
	{
		return nullptr;
	}

	FAssetPackageHeader Header;
	Ar << Header;

	if (!Header.IsValid(EAssetPackageType::ParticleSystem))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UParticleSystem* NewAsset = UObjectManager::Get().CreateObject<UParticleSystem>();
	if (!NewAsset)
	{
		return nullptr;
	}

	NewAsset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}

	NewAsset->SetSourcePath(NormalizedPath);
	NewAsset->BuildEmitters();

	LoadedParticleSystems.emplace(NormalizedPath, NewAsset);
	return NewAsset;
}

UParticleSystem* FParticleSystemManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedParticleSystems.find(NormalizedPath);
	return It != LoadedParticleSystems.end() ? It->second : nullptr;
}

bool FParticleSystemManager::Save(UParticleSystem* Asset)
{
	if (!Asset)
	{
		UE_LOG("[ParticleSystemManager] Save failed: Asset is NULL");
		return false;
	}

	const FString& Path = Asset->GetSourcePath();
	if (Path.empty())
	{
		UE_LOG("[ParticleSystemManager] Save failed: SourcePath is empty. Asset=%p", Asset);
		return false;
	}

	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	UE_LOG("[ParticleSystemManager] Save requested. SourcePath=%s NormalizedPath=%s Asset=%p EmitterCount=%d",
		Path.c_str(),
		NormalizedPath.c_str(),
		Asset,
		Asset->GetEmitterCount());

	FWindowsBinWriter Ar(NormalizedPath);
	if (!Ar.IsValid())
	{
		UE_LOG("[ParticleSystemManager] Save failed: file open failed. Path=%s", NormalizedPath.c_str());
		return false;
	}

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::ParticleSystem);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;

	Asset->BuildEmitters();
	Asset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UE_LOG("[ParticleSystemManager] Save failed: archive invalid after asset serialize. Path=%s", NormalizedPath.c_str());
		return false;
	}

	UE_LOG("[ParticleSystemManager] Save success. Path=%s", NormalizedPath.c_str());
	return true;
}

void FParticleSystemManager::RefreshAvailableParticleSystems()
{
	const std::filesystem::path ContentRoot =
		std::filesystem::path(FPaths::RootDir()) / L"Content" / L"Particle System";

	if (!std::filesystem::exists(ContentRoot))
	{
		AvailableParticleSystemFiles.clear();
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	AvailableParticleSystemFiles.clear();

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		std::wstring Ext = Entry.path().extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

		if (Ext != L".uasset")
		{
			continue;
		}

		const FString RelPath =
			FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::ParticleSystem, Metadata))
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
		Item.FullPath = RelPath;

		AvailableParticleSystemFiles.push_back(std::move(Item));
	}
}

const TArray<FAssetListItem>& FParticleSystemManager::GetAvailableParticleSystemFiles() const
{
	return AvailableParticleSystemFiles;
}
