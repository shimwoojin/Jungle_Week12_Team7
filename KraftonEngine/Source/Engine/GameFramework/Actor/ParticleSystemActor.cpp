#include "ParticleSystemActor.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleSystem.h"
#include "Particle/Modules/ParticleModuleLifetime.h"
#include "Particle/Modules/ParticleModuleLocation.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSize.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/Modules/ParticleModuleVelocity.h"
#include "Particle/TypeData/ParticleModuleTypeDataMesh.h"

// ---------------------------------------------------------------------------
// Runtime demo template builder — P1 자산 직렬화 / P2 에디터 완성 전까지의 우회용.
// Sprite emitter 1개 + Mesh emitter 1개 — 멀티 emitter 동시 렌더 시연 가능.
// 모듈 파라미터를 코드에서 직접 박음 (자산 파일 X → 코드 변경 시 다시 빌드 필요).
// ---------------------------------------------------------------------------
namespace
{
	void ConfigureSpriteEmitter(UParticleEmitter* Emitter)
	{
		if (!Emitter) return;
		Emitter->EmitterName = "Demo Sprite";

		UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
		if (!LOD) return;

		// Required: Sprite Material + (옵션) SubUV 그리드
		if (UParticleModuleRequired* Required = LOD->RequiredModule)
		{
			Required->MaterialSlot       = "Content/Material/Particle/ParticleSprite.uasset";
			Required->SubImagesHorizontal = 1;
			Required->SubImagesVertical   = 1;
			Required->bUseLocalSpace      = false;
		}

		// Spawn rate
		if (UParticleModuleSpawn* Spawn = LOD->SpawnModule)
		{
			Spawn->Rate = 50.0f;
		}

		// LOD0의 일반 모듈들 (Lifetime / Location / Velocity / Color / Size)
		// InitializeDefaultLODLevel이 추가했으니 Cast로 찾아서 파라미터만 갱신.
		for (UParticleModule* M : LOD->Modules)
		{
			if (auto* Lifetime = Cast<UParticleModuleLifetime>(M))
			{
				Lifetime->MinLifetime = 1.5f;
				Lifetime->MaxLifetime = 3.0f;
			}
			else if (auto* Loc = Cast<UParticleModuleLocation>(M))
			{
				Loc->StartLocationMin = { -0.3f, -0.3f, 0.0f };
				Loc->StartLocationMax = {  0.3f,  0.3f, 0.0f };
			}
			else if (auto* Vel = Cast<UParticleModuleVelocity>(M))
			{
				Vel->StartVelocityMin = { -0.5f, -0.5f, 1.0f };
				Vel->StartVelocityMax = {  0.5f,  0.5f, 3.0f };
			}
			else if (auto* Sz = Cast<UParticleModuleSize>(M))
			{
				Sz->StartSizeMin = { 0.2f, 0.2f, 0.2f };
				Sz->StartSizeMax = { 0.4f, 0.4f, 0.4f };
			}
		}
	}

	void ConfigureMeshEmitter(UParticleEmitter* Emitter)
	{
		if (!Emitter) return;
		Emitter->EmitterName = "Demo Mesh";

		UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
		if (!LOD) return;

		// TypeData = Mesh — 자동으로 FParticleMeshEmitterInstance 생성
		if (!LOD->TypeDataModule)
		{
			auto* TypeData = UObjectManager::Get().CreateObject<UParticleModuleTypeDataMesh>(LOD);
			if (TypeData)
			{
				TypeData->MeshSlot  = "Content/Data/BasicShape/Cube.OBJ";
				LOD->TypeDataModule = TypeData;
			}
		}

		// Required: Mesh Material — instance(2x2 atlas) 사용해 UMaterialInstance 경로 시연.
		// Parent: ParticleMesh.mat → SubImagesH/V만 override.
		if (UParticleModuleRequired* Required = LOD->RequiredModule)
		{
			Required->MaterialSlot       = "Content/Material/Particle/ParticleMesh_Atlas2x2.uasset";
			Required->SubImagesHorizontal = 2;
			Required->SubImagesVertical   = 2;
			Required->bUseLocalSpace      = false;
		}

		// Spawn rate — mesh는 더 적게
		if (UParticleModuleSpawn* Spawn = LOD->SpawnModule)
		{
			Spawn->Rate = 8.0f;
		}

		for (UParticleModule* M : LOD->Modules)
		{
			if (auto* Lifetime = Cast<UParticleModuleLifetime>(M))
			{
				Lifetime->MinLifetime = 2.5f;
				Lifetime->MaxLifetime = 5.0f;
			}
			else if (auto* Loc = Cast<UParticleModuleLocation>(M))
			{
				Loc->StartLocationMin = { -1.0f, -1.0f, 0.0f };
				Loc->StartLocationMax = {  1.0f,  1.0f, 0.0f };
			}
			else if (auto* Vel = Cast<UParticleModuleVelocity>(M))
			{
				Vel->StartVelocityMin = { 0.0f, 0.0f, 0.5f };
				Vel->StartVelocityMax = { 0.0f, 0.0f, 1.5f };
			}
			else if (auto* Sz = Cast<UParticleModuleSize>(M))
			{
				Sz->StartSizeMin = { 0.3f, 0.3f, 0.3f };
				Sz->StartSizeMax = { 0.5f, 0.5f, 0.5f };
			}
		}
	}

	// 데모 Template 빌드 — 새 UParticleSystem에 Sprite/Mesh emitter 2개 채워 반환.
	void BuildDemoTemplate(UParticleSystem* PS)
	{
		if (!PS) return;

		// (1) Sprite — billboard 빌보드 입자
		UParticleEmitter* Sprite = PS->AddEmitter();
		ConfigureSpriteEmitter(Sprite);

		// (2) Mesh — Cube 인스턴싱
		UParticleEmitter* Mesh = PS->AddEmitter();
		ConfigureMeshEmitter(Mesh);

		// BuildEmitters는 PSC가 호출할 거지만 모듈 layout 캐시 + offset 미리 계산 안전망.
		PS->BuildEmitters();
	}
}

void AParticleSystemActor::InitDefaultComponents()
{
	ParticleSystemComponent = AddComponent<UParticleSystemComponent>();
	SetRootComponent(ParticleSystemComponent);

	UParticleSystem* PS = UObjectManager::Get().CreateObject<UParticleSystem>();
	// BuildDemoTemplate(PS);

	ParticleSystemComponent->SetTemplate(PS);
	ParticleSystemComponent->Activate(true);
}

void AParticleSystemActor::PostDuplicate()
{
	AActor::PostDuplicate();
	ParticleSystemComponent = Cast<UParticleSystemComponent>(GetRootComponent());
}

void AParticleSystemActor::BeginPlay()
{
	AActor::BeginPlay();
}
