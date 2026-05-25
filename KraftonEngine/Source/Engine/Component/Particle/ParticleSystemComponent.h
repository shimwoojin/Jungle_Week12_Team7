#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleEvents.h"
#include "Core/Delegate.h"

#include "Source/Engine/Component/Particle/ParticleSystemComponent.generated.h"

class UParticleSystem;
class FParticleEmitterInstance;
class AParticleEventManager;
class UParticleSystemComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FParticleSystemFinishedSignature, UParticleSystemComponent* /*PSC*/);

// =============================================================================
// UParticleSystemComponent (PSC)
//   월드에 배치되어 UParticleSystem 에셋을 재생/렌더하는 컴포넌트.
//   - 매 tick 모든 emitter instance 를 Tick
//   - tick 끝에 DynamicEmitterData 목록을 모아 SceneProxy 로 전송
//   - 이벤트는 AParticleEventManager 로 디스패치
// =============================================================================
UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()
	UParticleSystemComponent();
	~UParticleSystemComponent() override;

	// --- 에셋 ---
	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }

	// --- 재생 제어 ---
	void Activate(bool bReset = false);
	void Deactivate();
	void ResetParticles();
	bool IsActive() const { return bActive; }

	// --- 컴포넌트 라이프사이클 ---
	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void CreateRenderState()  override;
	void DestroyRenderState() override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void PostEditProperty(const char* PropertyName) override;
	void PostDuplicate() override;
	void Serialize(class FArchive& Ar) override;

	// --- Bounds ---
	void UpdateWorldAABB() const override;

	// --- 이벤트 (외부 바인딩) ---
	FParticleSystemFinishedSignature OnSystemFinished;

	void SetEventManager(AParticleEventManager* InMgr) { EventManager = InMgr; }
	AParticleEventManager* GetEventManager() const     { return EventManager; }

	// --- Emitter Instance 접근 ---
	int32 GetEmitterInstanceCount() const { return static_cast<int32>(EmitterInstances.size()); }
	FParticleEmitterInstance* GetEmitterInstance(int32 Index) const;

	void RebuildInstances(bool bReset = true);
	const FString& GetTemplatePath() const { return TemplatePath.ToString(); }

	// SceneProxy 가 매 프레임 쓸 dynamic data 묶음. PSC 가 소유, GetDynamicData() 가
	// 매 호출마다 새로 build 한다 (호출자는 delete 책임).
	struct FDynamicData
	{
		TArray<FDynamicEmitterDataBase*> Emitters;
		~FDynamicData()
		{
			for (auto* E : Emitters) delete E;
		}
	};
	FDynamicData* BuildDynamicData();

protected:
	void CreateEmitterInstances();
	void DestroyEmitterInstances();
	void DispatchEventsToManager();
	// PSC는 EventManager를 직접 탐색/생성하지 않고, 상위 particle runtime provider가
	// 등록한 default manager를 이 helper로 주입받는다. EventManager는 basic playback/rendering에는
	// 필수가 아니지만, runtime gameplay가 외부 particle event delivery를 기대한다면 soft requirement다.
	// nullptr도 유효한 미주입 상태이며, 이 경우 PSC는 provider 상태를 다시 동기화할 수 있다.
	void RefreshEventManagerBinding();
	void ApplyCurrentLODToEmitterInstances();
	bool IsSystemFinished() const;
	void LoadTemplateFromPath();

	void PushDynamicDataToProxy();

protected:
	UParticleSystem* Template = nullptr;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Template", AssetType="UParticleSystem")
	FSoftObjectPtr TemplatePath = "None";

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Auto Activate")
	bool bAutoActivate = true;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Reset On Activate")
	bool bResetOnActivate = false;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Tick Interval (sec)")
	float TickInterval = 0.0f; // 0 = every frame

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="LOD Level")
	int32 CurrentLODIndex = 0;

	// 런타임 상태
	bool bActive = false;
	float AccumulatedTime = 0.0f;

	// emitter 인스턴스 — PSC 가 owning.
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// Higher-level particle runtime system이 주입하는 non-owning dependency.
	// PSC는 이 manager를 직접 찾거나 만들지 않는다.
	// EventManager가 없어도 particle playback/rendering은 계속 가능하지만,
	// 외부 gameplay/event-delivery use case는 manager registration을 기대한다.
	AParticleEventManager* EventManager = nullptr;
	bool bHasWarnedMissingEventManager = false;
	float MissingEventManagerTimeSeconds = 0.0f;

	// PSC 가 매 프레임 누적한 이벤트 (모든 emitter merge).
	struct FPendingEvents
	{
		TArray<FParticleEventSpawnData>    Spawn;
		TArray<FParticleEventDeathData>    Death;
		TArray<FParticleEventCollideData>  Collision;
		TArray<FParticleEventBurstData>    Burst;
	};
	FPendingEvents PendingEvents;
};
