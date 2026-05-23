#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/VertexTypes.h"     // Windows.h 를 끌어오므로 RenderStateTypes 보다 먼저
#include "Render/Types/RenderStateTypes.h"

// =============================================================================
// ParticleHelper.h
//   Particle 시스템 전역에서 공유되는 POD/구조체/상수 모음.
//   - FBaseParticle: 모든 입자가 공통으로 갖는 헤더 (alignment-친화 layout).
//   - Payload 매크로/엑세서: 모듈이 require 하는 추가 바이트를 BaseParticle 뒤에
//     이어 붙여 사용하기 위한 도우미.
//   - FDynamicEmitterData* / F*ReplayData*: GameThread → RenderThread 로
//     전달되는 immutable snapshot. ReplayData 는 직렬화/복기 (replay) 까지 지원.
//   - Constants: 정렬, 최대 입자 수, sprite/mesh draw 상수 등.
//   ※ 본 헤더는 UObject 를 직접 포함하지 않는다. (engine/render 양쪽에서 include)
// =============================================================================

class UMaterial;
class UStaticMesh;
class FParticleEmitterInstance;
class FParticleSystemSceneProxy;
class FParticleVertexFactory;

// -----------------------------------------------------------------------------
// 상수
// -----------------------------------------------------------------------------
namespace ParticleConstants
{
	// 모든 ParticleData 할당의 정렬 단위 (SIMD/cache line 친화).
	constexpr uint32 ParticleDataAlignment = 16;

	// PSC 1개가 보유할 수 있는 활성 입자 상한 (안전 가드).
	constexpr uint32 MaxParticlesPerEmitter = 16384;

	// Spawn 모듈이 매 프레임 누적할 수 있는 최대 보너스 (burst safety).
	constexpr uint32 MaxBurstCountPerFrame = 4096;
}

// -----------------------------------------------------------------------------
// FBaseParticle
//   모든 입자의 공통 헤더. 정확한 layout (offset) 이 Update/Render 양쪽에서
//   고정되어야 함. Module 이 추가 payload 를 원하면 RequiredBytes() 만큼
//   이 구조체 뒤에 이어 붙여 EmitterInstance 가 관리한다.
// -----------------------------------------------------------------------------
struct FBaseParticle
{
	FVector  Location          = { 0, 0, 0 };
	FVector  OldLocation       = { 0, 0, 0 };
	FVector  Velocity          = { 0, 0, 0 };
	FVector  BaseVelocity      = { 0, 0, 0 };
	FVector4 Color             = { 1, 1, 1, 1 };
	FVector4 BaseColor         = { 1, 1, 1, 1 };
	FVector  Size              = { 1, 1, 1 };
	FVector  BaseSize          = { 1, 1, 1 };
	float    Rotation          = 0.0f;
	float    BaseRotation      = 0.0f;
	float    RotationRate      = 0.0f;
	float    BaseRotationRate  = 0.0f;
	float    RelativeTime      = 0.0f; // 0..1, 1 도달 시 kill
	float    OneOverMaxLifetime = 1.0f;
	uint32   Flags             = 0;
	int32    SubImageIndex     = 0;    // SubUV 프레임 인덱스
};

// 입자 상태 플래그
enum class EParticleStateFlags : uint32
{
	None    = 0,
	Active  = 1u << 0,
	Spawned = 1u << 1, // 이번 프레임 생성된 입자 (Spawn 모듈만 Update)
	Killed  = 1u << 2,
};

// -----------------------------------------------------------------------------
// Payload 헬퍼
//   Module 이 RequiredBytes() 로 알려준 크기는 BaseParticle 뒤에 연속 배치된다.
//   EmitterInstance 가 ModuleOffsetMap 으로 (module → byte offset) 를 관리하고,
//   Spawn/Update 시 PARTICLE_PAYLOAD 매크로로 typed 포인터를 얻는다.
// -----------------------------------------------------------------------------
#define PARTICLE_PAYLOAD(Particle, Offset, Type) \
	reinterpret_cast<Type*>(reinterpret_cast<uint8*>(Particle) + (Offset))

#define PARTICLE_PAYLOAD_CONST(Particle, Offset, Type) \
	reinterpret_cast<const Type*>(reinterpret_cast<const uint8*>(Particle) + (Offset))

// 자주 쓰는 payload struct 들. 모듈이 자기 자신의 payload 를 정의할 수도 있다.
struct FParticlePayloadLocation { FVector InitialLocation; };
struct FParticlePayloadVelocity { FVector InitialVelocity; };
struct FParticlePayloadColor    { FVector4 InitialColor; };
struct FParticlePayloadSize     { FVector InitialSize; };

// -----------------------------------------------------------------------------
// Sprite / Mesh 정점 (Dynamic VB 에 기록됨)
// -----------------------------------------------------------------------------
struct FParticleSpriteVertex
{
	FVector  Position;
	FVector4 Color;
	FVector2 Size;       // (X=width, Y=height) world units
	float    Rotation;   // radians
	FVector2 UV;
	int32    SubImageIndex;
};

struct FParticleMeshInstanceVertex
{
	// per-instance transform (column-major 12 floats = 3 rows of float4)
	FVector4 Transform0;
	FVector4 Transform1;
	FVector4 Transform2;
	FVector4 Color;
	int32    SubImageIndex;
};

struct FParticleBeamTrailVertex
{
	FVector  Position;
	FVector4 Color;
	FVector2 UV;
};

// -----------------------------------------------------------------------------
// Dynamic Emitter Data 계층
//   GameThread 측 EmitterInstance 가 매 프레임 만들어 RenderThread 의
//   SceneProxy 로 넘긴다. 한 번 채워지면 immutable.
//   ReplayData 는 (FDynamicEmitterReplayDataBase) 이걸 그대로 직렬화/리플레이
//   할 수 있도록 분리한 plain struct. DynamicData 는 ReplayData + transient 캐시.
// -----------------------------------------------------------------------------

enum class EDynamicEmitterType : uint8
{
	Unknown = 0,
	Sprite,
	Mesh,
	Beam,
	Ribbon,
	Count,	// 배열 크기 산출용 — 새 타입 추가 시 이 위에.
};

struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType EmitterType = EDynamicEmitterType::Unknown;

	uint32 ActiveParticleCount = 0;
	uint32 ParticleStride      = 0;            // BaseParticle + payload
	TArray<uint8>  ParticleData;               // [ActiveParticleCount * Stride]
	TArray<uint16> ParticleIndices;            // 활성 인덱스 (sort 결과 등 포함 가능)

	// Material이 BlendState/DepthStencilState 등 렌더 상태의 single source of truth.
	// 별도 BlendState 필드를 두지 않음 — Material->GetBlendState() 사용.
	UMaterial* Material = nullptr;
	bool bUseLocalSpace = false;
	FMatrix LocalToWorld;      // bUseLocalSpace == true 일 때만 의미 있음 (default ctor = zero)
};

struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;
	virtual EDynamicEmitterType GetType() const = 0;
	virtual const FDynamicEmitterReplayDataBase& GetReplayDataBase() const = 0;
};

// -- Sprite ----
struct FDynamicSpriteEmitterReplayData : FDynamicEmitterReplayDataBase
{
	int32 SubImagesHorizontal = 1;
	int32 SubImagesVertical   = 1;
};

struct FDynamicSpriteEmitterData : FDynamicEmitterDataBase
{
	FDynamicSpriteEmitterReplayData Source;
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Sprite; }
	const FDynamicEmitterReplayDataBase& GetReplayDataBase() const override { return Source; }
};

// -- Mesh ----
struct FDynamicMeshEmitterReplayData : FDynamicEmitterReplayDataBase
{
	UStaticMesh* Mesh = nullptr;
};

struct FDynamicMeshEmitterData : FDynamicEmitterDataBase
{
	FDynamicMeshEmitterReplayData Source;
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Mesh; }
	const FDynamicEmitterReplayDataBase& GetReplayDataBase() const override { return Source; }
};

// -- Beam ----
struct FDynamicBeamEmitterReplayData : FDynamicEmitterReplayDataBase
{
	int32 InterpolationPoints = 0;
	FVector SourcePoint = { 0, 0, 0 };
	FVector TargetPoint = { 0, 0, 0 };
};

struct FDynamicBeamEmitterData : FDynamicEmitterDataBase
{
	FDynamicBeamEmitterReplayData Source;
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Beam; }
	const FDynamicEmitterReplayDataBase& GetReplayDataBase() const override { return Source; }
};

// -- Ribbon ----
struct FDynamicRibbonEmitterReplayData : FDynamicEmitterReplayDataBase
{
	int32 MaxTessellation = 1;
};

struct FDynamicRibbonEmitterData : FDynamicEmitterDataBase
{
	FDynamicRibbonEmitterReplayData Source;
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Ribbon; }
	const FDynamicEmitterReplayDataBase& GetReplayDataBase() const override { return Source; }
};

// -----------------------------------------------------------------------------
// FParticleSystemReplayFrame
//   한 프레임 PSC 전체 상태 (모든 emitter 의 ReplayData 묶음). 디버깅/리플레이 용도.
// -----------------------------------------------------------------------------
struct FParticleSystemReplayFrame
{
	float TimeSeconds = 0.0f;
	TArray<FDynamicEmitterReplayDataBase*> Emitters; // owned by frame
	~FParticleSystemReplayFrame()
	{
		for (auto* E : Emitters) delete E;
	}
};
