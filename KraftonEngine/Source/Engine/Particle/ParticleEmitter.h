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
//   CacheEmitterModuleInfo() 는 모든 LOD 의 모듈을 훑어 payload layout 을 계산한다.
//   런타임에서 LOD가 바뀌어도 module payload offset 이 존재해야 하기 때문이다.
//   즉 현재 런타임은 여전히 concrete/materialized UParticleLODLevel graph 와
//   module pointer identity에 강하게 결합되어 있다. 미래 effective-runtime LOD
//   materialization은 이 계약을 대체하거나 인접 단계에서 함께 빌드되어야 한다.
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

	// Asset-side stored LOD graph. Runtime still consumes these fully materialized
	// LOD objects directly today. The longer-term direction is for stored data to
	// become more compact/override-oriented while runtime may consume a separate
	// built effective LOD representation, but that split is not active yet.
	UPROPERTY(Save, Instanced, Category="LOD", DisplayName="LOD Levels", Type=Array, AllowedClass=UParticleLODLevel)
	TArray<UParticleLODLevel*> LODLevels;

	// 로드 후 코어 모듈 보장 + 캐시 재구성 (반사 직렬화는 UObject 템플릿이 자동 처리).
	void OnPostLoad(class FArchive& Ar) override;
	void PostDuplicate() override;

	// 새 Emitter생성 시 1회 호출
	void InitializeDefaultLODLevel();
	// 모든 LOD의 필수 슬롯을 보장한다. LOD가 하나도 없으면 LOD 0을 생성한다.
	void EnsureLODCoreModules();
	// Legacy compatibility wrapper. 새 코드에서는 EnsureLODCoreModules()를 사용한다.
	void EnsureLOD0CoreModules();

	// 새 LOD level 을 생성. 현재는 Required + Spawn 기본 모듈만 생성.
	// Derived LOD는 이후 SynchronizeDerivedLODFromLOD0() / UpdateFromLOD0()
	// 경로에서 inherited data resync, override preservation, reduction reapply,
	// and full-copy fallback policy를 적용받는다.
	// The resulting stored LOD is still a concrete/materialized graph because the
	// active runtime path consumes UParticleLODLevel objects directly today.
	// Authoring interpretation:
	// - resync from LOD0 refreshes inherited data
	// - explicit overrides remain local to the derived LOD
	UParticleLODLevel* CreateLODLevel(int32 InLevel);
	void               SynchronizeDerivedLODFromLOD0(UParticleLODLevel* DerivedLOD);
	void               RemoveLODLevel(int32 InLevel);
	UParticleLODLevel* GetLODLevel(int32 InLevel) const;
	UParticleLODLevel* GetCurrentLODLevel(int32 InCurrentLODIdx) const; // 안전 clamp

	int32 GetLODCount() const { return static_cast<int32>(LODLevels.size()); }

	// 모듈을 훑어 입자 1개의 총 크기 / Module → byte offset 을 계산.
	// PSC 가 EmitterInstance 를 만들 때 1회 호출되어야 한다.
	// 모듈 변경 시 PSC 가 다시 호출한다.
	// Today this cache is built against the concrete/materialized LOD graphs
	// stored on the emitter. Payload layout and module-instance offset lookup
	// therefore still assume stable module object identity across the active
	// runtime path. A future effective-runtime LOD build step would likely need
	// to integrate before or alongside this cache construction.
	void CacheEmitterModuleInfo();

	uint32 GetParticleSize()                const { return CachedLayout.ParticleStride; }
	uint32 GetReqInstanceBytes()            const { return CachedLayout.InstancePayloadSize; }
	uint32 GetModuleOffset(const class UParticleModule* M) const;
	const TMap<const UParticleModule*, uint32>& GetModuleOffsetMap() const { return CachedLayout.ModuleOffsets; }
	const FParticleLayout& GetParticleLayout() const { return CachedLayout; }

	// 인스턴스 팩토리 — TypeData 에 따라 sprite/mesh/beam/ribbon instance 생성.
	// The current path still chooses the runtime instance type from the stored
	// materialized LOD graph. A future effective-runtime LOD representation could
	// supply the same decision without exposing long-lived asset-side modules.
	FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent);

protected:
	// CacheEmitterModuleInfo() 의 결과 — emitter 가 소유 (재계산 가능).
	FParticleLayout CachedLayout;
};
