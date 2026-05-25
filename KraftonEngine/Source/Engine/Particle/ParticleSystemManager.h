#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "ParticleSystem.h"
#include "Asset/AssetRegistry.h"

class AParticleEventManager;

class FParticleSystemManager : public TSingleton<FParticleSystemManager>
{
	friend class TSingleton<FParticleSystemManager>;

public:
	UParticleSystem* Load(const FString& Path);
	UParticleSystem* Find(const FString& Path) const;
	bool Save(UParticleSystem* Asset);

	void RefreshAvailableParticleSystems();
	const TArray<FAssetListItem>& GetAvailableParticleSystemFiles() const;

	// Higher-level runtime/bootstrap code registers the current default event manager here.
	// This manager is non-owning; ParticleSystemManager only exposes it for later PSC injection.
	// nullptr is a valid "not registered yet" state.
	void SetDefaultEventManager(AParticleEventManager* InManager);
	AParticleEventManager* GetDefaultEventManager() const;

private:
	TMap<FString, UParticleSystem*> LoadedParticleSystems;
	TArray<FAssetListItem> AvailableParticleSystemFiles;

	// Non-owning reference provided by a higher-level runtime/bootstrap layer.
	// ParticleSystemManager does not create/destroy it, and PSC will consume this provider API later.
	AParticleEventManager* DefaultEventManager = nullptr;
};
