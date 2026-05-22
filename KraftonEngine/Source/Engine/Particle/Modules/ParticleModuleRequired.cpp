#include "ParticleModuleRequired.h"

#include "Materials/Material.h"

void UParticleModuleRequired::SetToSensibleDefaults(UParticleEmitter* Owner) {}

UMaterial* UParticleModuleRequired::ResolveMaterial()
{
	// TODO: MaterialSlot 으로부터 MaterialManager 에서 resolve 후 캐시.
	return CachedMaterial;
}
