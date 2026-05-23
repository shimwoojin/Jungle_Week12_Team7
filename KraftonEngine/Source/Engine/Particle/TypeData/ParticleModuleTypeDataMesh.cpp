#include "ParticleModuleTypeDataMesh.h"

#include "Mesh/MeshManager.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Pipeline/Renderer.h"
#include "Runtime/Engine.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleSystemComponent* /*InComponent*/)
{
	// emitter::CreateInstance 가 Init() 호출하므로 여기선 생성만.
	return new FParticleMeshEmitterInstance();
}

UStaticMesh* UParticleModuleTypeDataMesh::ResolveMesh()
{
	if (CachedMesh) return CachedMesh;

	const FString& Path = MeshSlot.ToString();
	if (Path.empty() || Path == "None") return nullptr;

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device) return nullptr;

	CachedMesh = FMeshManager::LoadStaticMesh(Path, Device);
	return CachedMesh;
}
