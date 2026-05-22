#pragma once

#include "Object/Object.h"
#include "Particle/ParticleHelper.h"

#include "Source/Engine/Particle/ParticleModule.generated.h"

class FParticleEmitterInstance;
class UParticleLODLevel;
struct FBaseParticle;

// =============================================================================
// UParticleModule
//   모든 파티클 모듈의 베이스. 모듈은 (1) 새로 생성된 입자에 1회 적용되는
//   Spawn 동작, (2) 매 프레임 활성 입자에 적용되는 Update 동작을 갖는다.
//   추가로 입자별 payload 바이트 (RequiredBytes) 와 emitter 인스턴스 전역
//   payload (RequiredBytesPerInstance) 를 요청할 수 있다.
//
//   Emitter::CacheEmitterModuleInfo() 가 모든 모듈을 훑어 ParticleSize 와
//   ModuleOffsetMap 을 계산한다. 모듈 자신은 layout 을 모르고, 호출 시점에
//   주어진 Offset 만 사용한다.
// =============================================================================
UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY()
	UParticleModule() = default;
	~UParticleModule() override = default;

	// --- 카테고리/식별 ----------------------------------------------------------
	// 카테고리 enum (Required/Spawn/Lifetime/...) — 에디터 그룹화 & 동일 카테고리
	// 중복 방지에 사용된다.
	enum class EModuleCategory : uint8
	{
		None,
		Required,
		TypeData,
		Spawn,
		Lifetime,
		Location,
		Velocity,
		Color,
		Size,
		Rotation,
		Collision,
		Event,
		SubUV,
	};
	virtual EModuleCategory GetCategory() const { return EModuleCategory::None; }

	// 에디터에 표시될 이름 (한국어 OK). 기본은 GetClass()->GetName().
	virtual const char* GetDisplayName() const { return "Module"; }

	// 같은 LODLevel 안에서 단 1개만 허용되는 모듈인지 (Required, TypeData 등).
	virtual bool IsUnique() const { return false; }

	// --- 라이프사이클 동작 ------------------------------------------------------

	// 입자가 막 생성된 직후 1회 호출.
	//   Owner          : 호출한 emitter 인스턴스
	//   ModuleOffset   : 이 모듈의 payload 가 BaseParticle 끝에서 시작하는 byte offset
	//                    (모듈이 RequiredBytes()>0 일 때만 의미가 있음)
	//   SpawnTime      : 입자 RelativeTime 기준 0..1 (sub-frame interp 용)
	//   Particle       : 초기화 대상 입자 (BaseParticle 헤더, 뒤에 payload)
	virtual void Spawn(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	                   float SpawnTime, FBaseParticle* Particle) {}

	// 매 프레임 활성 입자 전체에 대해 호출.
	//   Owner          : 호출한 emitter 인스턴스
	//   ModuleOffset   : 위와 동일
	//   DeltaTime      : 프레임 델타 (seconds)
	virtual void Update(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	                    float DeltaTime) {}

	// 시뮬레이션 시작 시 1회 (LODLevel 활성화 시점).
	virtual void FinalUpdate(FParticleEmitterInstance* Owner, uint32 ModuleOffset,
	                         float DeltaTime) {}

	// --- 메모리 요구량 ----------------------------------------------------------

	// 입자 1개당 추가로 요구하는 payload 바이트 수.
	virtual uint32 RequiredBytes(UParticleLODLevel* LODLevel) const { return 0; }

	// EmitterInstance 단위 추가 메모리 (모듈 전역 state). 매 입자에 추가되지 않음.
	virtual uint32 RequiredBytesPerInstance() const { return 0; }

	// --- 정합성/기본값 ----------------------------------------------------------

	// 모듈 추가 직후 호출 — 합리적 기본값을 채운다. 에디터에서도 호출.
	virtual void SetToSensibleDefaults(class UParticleEmitter* Owner) {}

	// LOD 변경 시 (LOD 0 의 모듈로부터 본인 LOD 값 보간/추출).
	virtual void RefreshFromLOD0(const UParticleModule* SourceModule) {}

	// 에디터 enable 토글. 비활성 모듈은 Spawn/Update skip.
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool b) { bEnabled = b; }

protected:
	bool bEnabled = true;
};
