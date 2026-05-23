#pragma once

#include "Object/Object.h"

#include "Source/Engine/Particle/ParticleLODLevel.generated.h"

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;

// =============================================================================
// UParticleLODLevel
//   하나의 LOD 단계에 해당하는 모듈 집합.
//   Required / TypeData 는 단 1개씩만 허용되고 (IsUnique=true),
//   그 외 모듈은 Modules 배열에 자유롭게 추가된다.
//   LOD 0 이 기준이고 그 외 LOD 는 RefreshFromLOD0() 로 값을 추출/보간.
// =============================================================================
UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY()
	UParticleLODLevel() = default;
	~UParticleLODLevel() override = default;

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Level")
	int32 Level = 0;

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Enabled")
	bool bEnabled = true;

	// Required: emitter 의 필수 설정 (Material, Sub UV, Sort 등) — 항상 존재.
	UPROPERTY(Save, Instanced, Category="Modules", DisplayName="Required Module", Type=ObjectRef, AllowedClass=UParticleModuleRequired)
	UParticleModuleRequired* RequiredModule = nullptr;

	// Spawn: 분당 spawn rate / Burst — 항상 존재.
	UPROPERTY(Save, Instanced, Category="Modules", DisplayName="Spawn Module", Type=ObjectRef, AllowedClass=UParticleModuleSpawn)
	UParticleModuleSpawn* SpawnModule = nullptr;

	// TypeData: 입자가 Sprite/Mesh/Beam/Ribbon 중 무엇인지 결정. nullptr 이면 Sprite.
	UPROPERTY(Save, Instanced, Category="Modules", DisplayName="Type Data", Type=ObjectRef, AllowedClass=UParticleModuleTypeDataBase)
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	// 그 외 (Lifetime, Location, Velocity, Color, Size, Collision, EventGenerator, ...)
	UPROPERTY(Save, Instanced, Category="Modules", DisplayName="Modules", Type=Array, AllowedClass=UParticleModule)
	TArray<UParticleModule*> Modules;

	// 직렬화
	void Serialize(class FArchive& Ar) override;
	void PostDuplicate() override;

	// --- API ---
	// LOD 변경 시 LOD 0 으로부터 본인 값을 재추출.
	void UpdateFromLOD0(UParticleLODLevel* LOD0);

	// 동일 카테고리 중복/required 충돌 등을 검증. true 면 정상.
	bool ValidateModules() const;

	// 모듈 1개 추가/제거 (에디터 hook). 카테고리 정책 (Unique) 을 검사.
	bool AddModule(UParticleModule* InModule);
	bool RemoveModule(UParticleModule* InModule);

	// 동일 클래스 첫 번째 모듈을 찾는다.
	template<typename T>
	T* FindModuleByClass() const
	{
		for (UParticleModule* M : Modules)
		{
			if (T* Hit = Cast<T>(M))
			{
				return Hit;
			}
		}
		return nullptr;
	}
};
