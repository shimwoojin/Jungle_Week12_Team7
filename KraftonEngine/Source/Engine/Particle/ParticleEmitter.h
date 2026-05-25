#pragma once

#include "Object/Object.h"
#include "Particle/ParticleHelper.h"

#include "Source/Engine/Particle/ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;
class FParticleEmitterInstance;
class UParticleSystemComponent;

// =============================================================================
// UParticleEmitter
//   하나의 emitter (= LODLevels 의 묶음). PSC 가 emitter 하나마다 한 개의
//   FParticleEmitterInstance 를 만든다.
//   CacheEmitterModuleInfo() 에서 LOD0 의 모듈을 훑어 CachedLayout 을 계산해두면
//   EmitterInstance 가 그대로 사용한다.
// =============================================================================
UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY()
	UParticleEmitter() = default;
	~UParticleEmitter() override = default;

	UPROPERTY(Edit, Save, Category="Emitter", DisplayName="Emitter Name")
	FString EmitterName = "Emitter";

	UPROPERTY(Edit, Save, Category="Emitter", DisplayName="Enabled")
	bool bEnabled = true;

	UPROPERTY(Save, Instanced, Category="LOD", DisplayName="LOD Levels", Type=Array, AllowedClass=UParticleLODLevel)
	TArray<UParticleLODLevel*> LODLevels;

	// 로드 후 코어 모듈 보장 + 캐시 재구성 (반사 직렬화는 UObject 템플릿이 자동 처리).
	void OnPostLoad(class FArchive& Ar) override;
	void PostDuplicate() override;

	// 새 Emitter생성 시 1회 호출
	void InitializeDefaultLODLevel();
	// 필수 슬롯만 보장
	void EnsureLOD0CoreModules();

	// 새 LOD level 을 생성. 현재는 Required + Spawn 기본 모듈만 생성.
	// LOD0 모듈 복사/보간은 UpdateFromLOD0() 구현 시 처리.
	UParticleLODLevel* CreateLODLevel(int32 InLevel);
	void               RemoveLODLevel(int32 InLevel);
	UParticleLODLevel* GetLODLevel(int32 InLevel) const;
	UParticleLODLevel* GetCurrentLODLevel(int32 InCurrentLODIdx) const; // 안전 clamp

	int32 GetLODCount() const { return static_cast<int32>(LODLevels.size()); }

	// 모듈을 훑어 입자 1개의 총 크기 / Module → byte offset 을 계산.
	// PSC 가 EmitterInstance 를 만들 때 1회 호출되어야 한다.
	// 모듈 변경 시 PSC 가 다시 호출한다.
	void CacheEmitterModuleInfo();

	uint32 GetParticleSize()                const { return CachedLayout.ParticleStride; }
	uint32 GetReqInstanceBytes()            const { return CachedLayout.InstancePayloadSize; }
	uint32 GetModuleOffset(const class UParticleModule* M) const;
	const TMap<const UParticleModule*, uint32>& GetModuleOffsetMap() const { return CachedLayout.ModuleOffsets; }
	const FParticleLayout& GetParticleLayout() const { return CachedLayout; }

	// 인스턴스 팩토리 — TypeData 에 따라 sprite/mesh/beam/ribbon instance 생성.
	FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent);

protected:
	// CacheEmitterModuleInfo() 의 결과 — emitter 가 소유 (재계산 가능).
	FParticleLayout CachedLayout;
};
