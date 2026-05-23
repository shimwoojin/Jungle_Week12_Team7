#include "ParticleModuleTypeDataMesh.h"

#include "Engine/Runtime/Engine.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/Object.h"
#include "Particle/ParticleEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleSystemComponent* InComponent)
{
	(void)InComponent;
	// The normal emitter/PSC path performs Init(); TypeData only selects the runtime instance type.
	return new FParticleMeshEmitterInstance();
}

UStaticMesh* UParticleModuleTypeDataMesh::ResolveMesh()
{
	if (CachedMesh)
	{
		return CachedMesh;
	}

	if (UStaticMesh* CachedObject = Cast<UStaticMesh>(MeshSlot.Get()))
	{
		CachedMesh = CachedObject;
		return CachedMesh;
	}

	if (MeshSlot.IsNull() || MeshSlot == "None")
	{
		return nullptr;
	}

	if (!GEngine)
	{
		return nullptr;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return nullptr;
	}

	// Resolve the soft path once, cache it on the type-data object, and hand the same mesh to replay.
	CachedMesh = FMeshManager::LoadStaticMesh(MeshSlot.ToString(), Device);
	if (CachedMesh)
	{
		MeshSlot.SetCachedObject(CachedMesh);
	}

	return CachedMesh;
}
