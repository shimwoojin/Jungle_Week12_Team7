#include "ParticleModuleRequired.h"

#include "Materials/Material.h"

void UParticleModuleRequired::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	MaterialSlot = "None";
	CachedMaterial = nullptr;

	BlendState = EBlendState::AlphaBlend;
	bUseLocalSpace = false;

	SubImagesHorizontal = 1;
	SubImagesVertical = 1;

	EmitterDuration = 1.0f;
	EmitterLoops = 0;

	SortMode = ESortMode::None;
	ScreenAlignment = EScreenAlignment::Square;
}

UMaterial* UParticleModuleRequired::ResolveMaterial()
{
	// TODO: MaterialSlot 으로부터 MaterialManager 에서 resolve 후 캐시.
	return CachedMaterial;
}
