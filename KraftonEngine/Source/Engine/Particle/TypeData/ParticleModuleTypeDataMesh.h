#pragma once

#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Particle/TypeData/ParticleModuleTypeDataMesh.generated.h"

class UStaticMesh;

// =============================================================================
// UParticleModuleTypeDataMesh
//   Mesh Emitter — 각 입자가 StaticMesh 의 instance 로 렌더된다.
//   FParticleMeshEmitterInstance 를 생성하고, ReplayData 에 Mesh 포인터를 박는다.
// =============================================================================
UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataMesh() = default;

	const char* GetDisplayName() const override { return "TypeData (Mesh)"; }

	FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent) override;

	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Static Mesh", AssetType="StaticMesh")
	FSoftObjectPtr MeshSlot;
	UStaticMesh*   CachedMesh = nullptr; // ResolveMesh() 에서 resolve

	UStaticMesh* ResolveMesh();

	// 메시 정렬 모드 (속도/축/카메라).
	enum class EMeshAlignment : uint8 { None, Velocity, FacingCamera, AxisLock };
	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Alignment", Enum=EMeshAlignment)
	EMeshAlignment Alignment = EMeshAlignment::None;

	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Override Material")
	bool bOverrideMaterial = false;
};
