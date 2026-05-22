#include "ParticleModuleTypeDataMesh.h"

#include "Particle/ParticleEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleSystemComponent* InComponent)
{
	// TODO: new FParticleMeshEmitterInstance() — emitter::CreateInstance 가 Init() 호출.
	return nullptr;
}

UStaticMesh* UParticleModuleTypeDataMesh::ResolveMesh()
{
	// TODO: MeshSlot 으로 StaticMesh 리졸브.
	return CachedMesh;
}
