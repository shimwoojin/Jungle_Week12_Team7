#include "Editor/Subsystem/AssetFactory.h"

#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "FloatCurve/FloatCurveManager.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Platform/Paths.h"

#include <filesystem>

#include "Core/Logging/Log.h"
#include "Particle/ParticleSystemManager.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"

namespace
{
	FString SanitizeAssetStem(const FString& AssetName)
	{
		return AssetName.empty() ? FString("NewFloatCurve") : AssetName;
	}

	std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path& Directory, const FString& AssetName, const wchar_t* Extension)
	{
		const FString BaseStem = SanitizeAssetStem(AssetName);

		int32 Suffix = 0;
		for (;;)
		{
			FString CandidateStem = BaseStem;
			if (Suffix > 0)
			{
				CandidateStem += "_";
				CandidateStem += std::to_string(Suffix);
			}

			std::filesystem::path CandidatePath = Directory / (FPaths::ToWide(CandidateStem) + Extension);
			if (!std::filesystem::exists(CandidatePath))
			{
				return CandidatePath;
			}

			++Suffix;
		}
	}

}

bool FAssetFactory::CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	FFloatCurve& Curve = NewAsset->GetCurve();
	Curve.Reset();
	Curve.AddKey(0.0f, 0.0f);
	Curve.AddKey(1.0f, 1.0f);
	Curve.SortKeys();

	bool bSaved = FFloatCurveManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UCameraShakeAsset* NewAsset = UObjectManager::Get().CreateObject<UCameraShakeAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->Version = 1;
	NewAsset->ShakeType = ECameraShakeType::Sequence;

	bool bSaved = FCameraShakeManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateAnimGraph(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UAnimGraphAsset* NewAsset = UObjectManager::Get().CreateObject<UAnimGraphAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->InitializeDefault(); // SequencePlayer → OutputPose 기본 그래프.

	bool bSaved = FAnimGraphManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateParticleSystem(
	const FString& DirectoryPath,
	const FString& AssetName,
	FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		UE_LOG("[ParticleSystemFactory] Create failed: invalid directory. DirectoryPath=%s", DirectoryPath.c_str());
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(
		Directory,
		AssetName.empty() ? "NewParticleSystem" : AssetName,
		L".uasset"
	);
	const FString CreatedPath = FPaths::ToUtf8(AssetPath.wstring());

	UE_LOG("[ParticleSystemFactory] Create start. Directory=%s AssetName=%s CreatedPath=%s",
		DirectoryPath.c_str(),
		AssetName.c_str(),
		CreatedPath.c_str());

	UParticleSystem* NewAsset = UObjectManager::Get().CreateObject<UParticleSystem>();
	if (!NewAsset)
	{
		UE_LOG("[ParticleSystemFactory] CreateObject<UParticleSystem> returned NULL. Path=%s", CreatedPath.c_str());
		return false;
	}

	NewAsset->SetSourcePath(CreatedPath);
	NewAsset->AddEmitter();
	NewAsset->BuildEmitters();

	const bool bSaved = FParticleSystemManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		UE_LOG("[ParticleSystemFactory] Save failed. Path=%s", CreatedPath.c_str());
		return false;
	}

	UParticleSystem* Loaded = FParticleSystemManager::Get().Load(CreatedPath);
	if (!Loaded)
	{
		UE_LOG("[ParticleSystemFactory] Load FAILED after Save. Path=%s", CreatedPath.c_str());
		return false;
	}

	OutCreatedPath = CreatedPath;
	UE_LOG("[ParticleSystemFactory] Create success. Path=%s", OutCreatedPath.c_str());
	return true;
}