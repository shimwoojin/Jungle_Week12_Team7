#pragma once

#include "Object/Object.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/ParticleSystem.generated.h"

class UParticleEmitter;

// =============================================================================
// UParticleSystem
//   디스크 직렬화 단위의 파티클 에셋. 여러 UParticleEmitter 를 묶는다.
//   PSC 가 이 에셋을 참조하여 EmitterInstance 를 만든다.
// =============================================================================
UCLASS()
class UParticleSystem : public UObject
{
public:
	GENERATED_BODY()
	UParticleSystem() = default;
	~UParticleSystem() override = default;

	UPROPERTY(Edit, Save, Category="System", DisplayName="Emitters", Type=Array)
	TArray<UParticleEmitter*> Emitters;

	// 시스템 자체의 재시작 정책.
	UPROPERTY(Edit, Save, Category="System", DisplayName="Looping")
	bool bLooping = true;

	UPROPERTY(Edit, Save, Category="System", DisplayName="Update Time FPS", Min=10.0f, Max=120.0f)
	float UpdateTimeFPS = 60.0f;

	// 시스템 경계 박스 (모든 emitter 합산). 매 프레임 PSC 에서 갱신.
	UPROPERTY(Edit, Save, Category="System", DisplayName="System Bounds Min")
	FVector SystemBoundsMin = { -100, -100, -100 };
	UPROPERTY(Edit, Save, Category="System", DisplayName="System Bounds Max")
	FVector SystemBoundsMax = {  100,  100,  100 };

	UPROPERTY(Edit, Save, Category="System", DisplayName="Use Fixed Relative Bounding Box")
	bool bUseFixedRelativeBoundingBox = true;

	// 직렬화 / Duplicate
	void Serialize(class FArchive& Ar) override;
	UObject* Duplicate(UObject* NewOuter = nullptr) const override;

	// 에디터/런타임 헬퍼
	UParticleEmitter* AddEmitter();
	void              RemoveEmitter(int32 Index);
	void              MoveEmitter(int32 FromIndex, int32 ToIndex);

	int32 GetEmitterCount() const { return static_cast<int32>(Emitters.size()); }
	UParticleEmitter* GetEmitter(int32 Index) const;

	// 모든 emitter 의 CacheEmitterModuleInfo() 호출.
	void BuildEmitters();

private:
	FString MakeUniqueEmitterName();
};
