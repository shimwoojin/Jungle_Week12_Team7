#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "ParticleSystem.h"
#include "Asset/AssetRegistry.h"

class FParticleSystemManager : public TSingleton<FParticleSystemManager>
{
	friend class TSingleton<FParticleSystemManager>;

public:
	UParticleSystem* Load(const FString& Path);
	UParticleSystem* Find(const FString& Path) const;
	bool Save(UParticleSystem* Asset);

	void RefreshAvailableParticleSystems();
	const TArray<FAssetListItem>& GetAvailableParticleSystemFiles() const;

private:
	TMap<FString, UParticleSystem*> LoadedParticleSystems;
	TArray<FAssetListItem> AvailableParticleSystemFiles;
};