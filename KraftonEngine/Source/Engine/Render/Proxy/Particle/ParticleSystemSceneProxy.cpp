#include "ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Render/Particle/ParticleVertexFactory.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"

#include <vector>
#include <cmath>

// =============================================================================
// FParticleSystemSceneProxy
//   Day 3 마일스톤: 단색 빌보드 먼지 첫 DrawCall.
//   - DynamicData가 없으면 stub 8입자 (proxy origin 주변 2x2x2 격자)로 파이프라인 검증.
//   - UpdatePerViewport: 카메라 벡터만 캐시 (Device 없음).
//   - PrepareDrawBuffer: 입자 → 빌보드 corner 정점 expansion + VB/IB 업로드 (Device 사용).
// =============================================================================

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// 빌보드라 카메라가 움직이면 매 프레임 갱신 필요. 프러스텀 컬링은 일단 패스 (Day 6 ShowFlag에서 정교화).
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
	ProxyFlags |= EPrimitiveProxyFlags::Particle;       // ShowFlags.bParticles 토글 대상
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;

	// Factories는 GetOrCreateFactory가 lazy 생성. 여기서는 빈 배열.
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	for (FParticleVertexFactory*& F : Factories)
	{
		if (F) { F->ReleaseResources(); delete F; F = nullptr; }
	}
	delete DynamicData; DynamicData = nullptr;
	SpriteVB.Release();
	SpriteIB.Release();
	MeshInstanceVB.Release();
	// ParticleMaterial은 UObjectManager가 소유 — 여기서 해제 X
}

FParticleVertexFactory* FParticleSystemSceneProxy::GetOrCreateFactory(EDynamicEmitterType Type, ID3D11Device* Device) const
{
	const int Idx = (int)Type;
	if (Idx <= (int)EDynamicEmitterType::Unknown || Idx >= (int)EDynamicEmitterType::Count) return nullptr;
	if (Factories[Idx]) return Factories[Idx];

	switch (Type)
	{
	case EDynamicEmitterType::Sprite: Factories[Idx] = new FParticleSpriteVertexFactory(); break;
	case EDynamicEmitterType::Mesh:   Factories[Idx] = new FParticleMeshVertexFactory();   break;
	case EDynamicEmitterType::Beam:   Factories[Idx] = new FParticleBeamVertexFactory();   break;
	case EDynamicEmitterType::Ribbon: Factories[Idx] = new FParticleRibbonVertexFactory(); break;
	default: return nullptr;
	}
	Factories[Idx]->InitResources(Device);
	return Factories[Idx];
}

UParticleSystemComponent* FParticleSystemSceneProxy::GetPSC() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}

void FParticleSystemSceneProxy::SetDynamicData(UParticleSystemComponent::FDynamicData* InData)
{
	delete DynamicData;
	DynamicData = InData;
}

void FParticleSystemSceneProxy::UpdateTransform()
{
	// 입자 정점이 이미 월드 좌표라 Model은 Identity. CachedWorldPos만 정렬용으로 캐시.
	UPrimitiveComponent* Comp = GetOwner();
	if (Comp)
	{
		CachedWorldPos = Comp->GetWorldLocation();
		CachedBounds   = Comp->GetWorldBoundingBox();
	}
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	MarkPerObjectCBDirty();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	//if (!SpriteMaterial)
	//{
	//	SpriteMaterial = UMaterial::CreateTransient(
	//		ERenderPass::Translucent, EBlendState::AlphaBlend,
	//		EDepthStencilState::DepthReadOnly, ERasterizerState::SolidNoCull,
	//		FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite));
	//}
	//if (!MeshMaterial)
	//{
	//	MeshMaterial = UMaterial::CreateTransient(
	//		ERenderPass::Translucent, EBlendState::AlphaBlend,
	//		EDepthStencilState::DepthReadOnly, ERasterizerState::SolidBackCull,
	//		FShaderManager::Get().GetOrCreate(EShaderPath::ParticleMesh));
	//}

	//// Material/IndexCount는 PrepareDrawBuffer가 매 프레임 갱신 (emitter type 따라).
	//// 여기선 placeholder 1개만 등록 — Sprite를 default로.
	//SectionDraws.clear();
	//SectionDraws.push_back({ SpriteMaterial, /*FirstIndex*/0, /*IndexCount*/0 });
}

void FParticleSystemSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	// Particle 프록시는 정적 MeshBuffer 없음 — 매 프레임 dynamic VB/IB로 그림.
	MeshBuffer = nullptr;

	// 자산 기반 Material 로드 — Template + ConstantBufferMap 자동 빌드되어 SetScalarParameter 작동.
	// .mat에서 Opacity / BaseColor / RenderPass / BlendState 등 손쉽게 조정.
	if (!SpriteMaterial)
	{
		SpriteMaterial = FMaterialManager::Get().GetOrCreateMaterial(
			"Content/Material/Particle/ParticleSprite.mat");
	}
	if (!MeshMaterial)
	{
		MeshMaterial = FMaterialManager::Get().GetOrCreateMaterial(
			"Content/Material/Particle/ParticleMesh.mat");
	}

	// Template이 있으면 emitter index별 RequiredModule.Material 캐싱.
	// 없으면 EmitterMaterials는 비고 PrepareDrawBuffer가 fallback 사용.
	EmitterMaterials.clear();
	UParticleSystemComponent* PSC = GetPSC();
	if (UParticleSystem* Template = PSC ? PSC->GetTemplate() : nullptr)
	{
		const int32 Count = Template->GetEmitterCount();
		EmitterMaterials.reserve(static_cast<size_t>(Count));
		for (int32 i = 0; i < Count; ++i)
		{
			UMaterial* M = nullptr;
			UParticleModuleRequired* Required = nullptr;
			if (UParticleEmitter* Emitter = Template->GetEmitter(i))
			{
				// LOD 0 기준 — LOD 시스템 도입 시 CurrentLOD 사용.
				if (UParticleLODLevel* LOD = Emitter->GetCurrentLODLevel(0))
				{
					Required = LOD->RequiredModule;
					if (Required)
					{
						M = Required->ResolveMaterial();
					}
				}
			}

			// RequiredModule.SubImagesH/V를 Material cbuffer에 흘림 (Cascade 패턴).
			// Sprite Material엔 해당 파라미터 없으면 SetScalarParameter가 silent fail (무시).
			// 한계: 동일 Material 자산이 emitter끼리 공유되면 마지막 set이 모두 적용 — 추후 MaterialInstance로 분리 필요.
			if (M && Required)
			{
				M->SetScalarParameter("SubImagesH", static_cast<float>(Required->SubImagesHorizontal));
				M->SetScalarParameter("SubImagesV", static_cast<float>(Required->SubImagesVertical));
			}

			EmitterMaterials.push_back(M); // null도 push — index 1:1 유지, fallback 처리
		}
	}

	// Material/IndexCount는 PrepareDrawBuffer가 매 프레임 갱신 (emitter type 따라).
	// 여기선 placeholder 1개만 등록 — Sprite를 default로.
	SectionDraws.clear();
	if (SpriteMaterial)
	{
		SectionDraws.push_back({ SpriteMaterial, /*FirstIndex*/0, /*IndexCount*/0 });
	}
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible) return;

	CachedCameraRight    = Frame.CameraRight;
	CachedCameraUp       = Frame.CameraUp;
	CachedCameraPosition = Frame.CameraPosition;
	// Model = Identity 유지 (정점이 이미 월드 좌표).
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	MarkPerObjectCBDirty();
}

// ---------------------------------------------------------------------------
// 스텁 데이터 생성 — DynamicData가 없으면 proxy origin 주변에 8개 입자 배치.
// Day 4 이후 P1의 BuildDynamicData가 도착하면 이 경로는 죽음.
// ---------------------------------------------------------------------------
static void BuildStubReplay(FDynamicSpriteEmitterReplayData& OutReplay, const FVector& Origin)
{
	OutReplay.EmitterType = EDynamicEmitterType::Sprite;
	OutReplay.ActiveParticleCount = 8;
	OutReplay.ParticleStride = sizeof(FBaseParticle);
	OutReplay.bUseLocalSpace = false;
	// Material은 proxy의 transient ParticleMaterial이 BlendState 등 담당 — stub은 미설정.
	OutReplay.SnapshotStorage.Allocate(
		OutReplay.ActiveParticleCount * OutReplay.ParticleStride,
		OutReplay.ActiveParticleCount,
		0);

	FBaseParticle* P = reinterpret_cast<FBaseParticle*>(OutReplay.SnapshotStorage.ParticleData);

	// 2x2x2 격자 — proxy 위치 기준 ±1 유닛 (Day 3 시각 검증용)
	static const float Off[8][3] = {
		{ -1, -1, -1 }, {  1, -1, -1 }, { -1,  1, -1 }, {  1,  1, -1 },
		{ -1, -1,  1 }, {  1, -1,  1 }, { -1,  1,  1 }, {  1,  1,  1 },
	};
	static const FVector4 Colors[8] = {
		{ 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 0, 1 },
		{ 1, 0, 1, 1 }, { 0, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 0.5f, 0, 1 },
	};

	for (uint32 i = 0; i < 8; ++i)
	{
		new (&P[i]) FBaseParticle();
		P[i].Location = Origin + FVector(Off[i][0], Off[i][1], Off[i][2]);
		P[i].Size     = { 0.3f, 0.3f, 0.3f };
		P[i].Color    = Colors[i];
		P[i].BaseColor = Colors[i];
		P[i].BaseSize  = P[i].Size;
		P[i].RelativeTime = 0.0f;
		P[i].OneOverMaxLifetime = 1.0f;
	}
}

// ---------------------------------------------------------------------------
// Mesh stub — Replay.Mesh = nullptr → factory가 엔진 빌트인 Cube로 fallback.
// Count 파라미터로 1만 입자 성능 측정 시 늘릴 수 있음.
// 배치: sunflower seed pattern (golden angle spiral) — 입자가 겹치지 않으면서 disk 분포.
// ---------------------------------------------------------------------------
static void BuildStubMeshReplay(FDynamicMeshEmitterReplayData& OutReplay,
                                 const FVector& Origin, uint32 Count)
{
	OutReplay.EmitterType = EDynamicEmitterType::Mesh;
	OutReplay.ActiveParticleCount = Count;
	OutReplay.ParticleStride = sizeof(FBaseParticle);
	OutReplay.bUseLocalSpace = false;
	OutReplay.Mesh = nullptr; // factory가 Cube fallback
	OutReplay.SnapshotStorage.Allocate(
		Count * OutReplay.ParticleStride,
		Count,
		0);

	FBaseParticle* P = reinterpret_cast<FBaseParticle*>(OutReplay.SnapshotStorage.ParticleData);

	// Golden angle spiral — disk 패턴. 큰 N에서도 균등 분포.
	constexpr float GoldenAngle = 2.39996323f;
	// 1만 입자에서 size 0.1로 화면 가득 — 측정용.
	const float ParticleSize = Count > 1000 ? 0.1f : 0.5f;
	const float SpacingFactor = Count > 1000 ? 0.15f : 0.6f;

	for (uint32 i = 0; i < Count; ++i)
	{
		new (&P[i]) FBaseParticle();
		const float Phi    = static_cast<float>(i) * GoldenAngle;
		const float Radius = SpacingFactor * std::sqrt(static_cast<float>(i + 1));
		const float Cosp   = std::cos(Phi);
		const float Sinp   = std::sin(Phi);

		P[i].Location  = Origin + FVector(Cosp * Radius, Sinp * Radius, 0.0f);
		P[i].Size      = { ParticleSize, ParticleSize, ParticleSize };
		P[i].BaseSize  = P[i].Size;
		// 색상은 phase 따라 회전
		P[i].Color     = FVector4{ 0.5f + 0.5f * Cosp, 0.5f + 0.5f * Sinp, 0.5f, 1.0f };
		P[i].BaseColor = P[i].Color;
		P[i].Rotation  = 0.0f;
		P[i].RelativeTime = 0.0f;
		P[i].OneOverMaxLifetime = 1.0f;
	}
}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                  FDrawCommandBuffer& OutBuffer) const
{
	OutBuffer = {}; // section-level BufferOverride 사용 — ProxyBuffer는 빈 상태로 둠
	if (!Device || !Context) return false;

	// ---- Replay 목록 결정 (실제 DynamicData 우선, 없으면 stub은 Sprite + Mesh 둘 다) ----
	FDynamicSpriteEmitterReplayData StubSpriteReplay;
	FDynamicMeshEmitterReplayData   StubMeshReplay;
	TArray<const FDynamicEmitterReplayDataBase*> Replays;
	if (DynamicData && !DynamicData->Emitters.empty())
	{
		for (FDynamicEmitterDataBase* E : DynamicData->Emitters)
		{
			if (E) Replays.push_back(&E->GetReplayDataBase());
		}
	}
	if (Replays.empty())
	{
		// 멀티 emitter 동시 렌더 검증: Sprite + Mesh stub 둘 다 push.
		// 성능 측정 시 STUB_MESH_COUNT를 10000으로 변경해서 1만 입자 16ms 게이트 확인.
		constexpr uint32 STUB_MESH_COUNT = 10000;
		BuildStubMeshReplay(StubMeshReplay, CachedWorldPos, STUB_MESH_COUNT);
		Replays.push_back(&StubMeshReplay);
		BuildStubReplay(StubSpriteReplay, CachedWorldPos);
		Replays.push_back(&StubSpriteReplay);
	}

	// ---- 모든 emitter 디스패치 — emitter당 1 SectionDraw + 자체 BufferOverride ----
	auto& MutableSections = const_cast<TArray<FMeshSectionDraw>&>(GetSectionDraws());
	MutableSections.clear();
	uint32 TotalIndexCount = 0;
	uint32 EmitterIdx = 0;

	for (const FDynamicEmitterReplayDataBase* Replay : Replays)
	{
		FParticleVertexFactory* Factory = GetOrCreateFactory(Replay->EmitterType, Device);
		if (!Factory) { ++EmitterIdx; continue; }

		// emitter type별 dedicated VB (Sprite/Mesh 정점 포맷 다르니 공유 불가)
		FDynamicVertexBuffer* EmitterVB = nullptr;
		switch (Replay->EmitterType)
		{
		case EDynamicEmitterType::Sprite: EmitterVB = &SpriteVB;       break;
		case EDynamicEmitterType::Mesh:   EmitterVB = &MeshInstanceVB; break;
		default: ++EmitterIdx; continue; // Beam/Ribbon 미구현
		}

		// Material 먼저 결정 — sort 조건 산출 위해 BuildDraw 호출 전에 BlendState 확인.
		// RequiredModule.Material 우선 (Template 있을 때). 없으면 type별 fallback.
		UMaterial* RequiredMat = (EmitterIdx < EmitterMaterials.size())
			? EmitterMaterials[EmitterIdx] : nullptr;
		UMaterial* FallbackMat = (Replay->EmitterType == EDynamicEmitterType::Mesh)
			? MeshMaterial : SpriteMaterial;
		UMaterial* SectionMat  = RequiredMat ? RequiredMat : FallbackMat;

		// 정렬 필요 여부는 이제 material blend가 아니라 replay 계약의 SortMode가 결정한다.
		const bool bRequiresSort = Replay->SortMode != EParticleReplaySortMode::None;

		FParticleVertexFactory::FDrawSpec Spec;
		if (!Factory->BuildDraw(Device, Context, *Replay,
			CachedCameraRight, CachedCameraUp, CachedCameraPosition,
			bRequiresSort, Replay->SortMode, *EmitterVB, Spec))
		{
			++EmitterIdx;
			continue;
		}
		if (Spec.IndexCount == 0) { ++EmitterIdx; continue; }

		FMeshSectionDraw Section;
		Section.FirstIndex = 0;
		Section.IndexCount = Spec.IndexCount;
		Section.Material   = SectionMat;

		if (Spec.InstanceCount > 0)
		{
			// Mesh: 정적 VB(slot 0) + IB + dynamic instance VB(slot 1)
			Section.BufferOverride.VB               = Spec.StaticVB;
			Section.BufferOverride.VBStride         = Spec.StaticVBStride;
			Section.BufferOverride.IB               = Spec.StaticIB;
			Section.BufferOverride.InstanceCount    = Spec.InstanceCount;
			Section.BufferOverride.InstanceVB       = EmitterVB->GetBuffer();
			Section.BufferOverride.InstanceVBStride = EmitterVB->GetStride();
		}
		else
		{
			// Sprite: 동적 VB + 동적 quad IB 생성
			const uint32 NumQuads = Spec.IndexCount / 6;
			std::vector<uint32> Indices(Spec.IndexCount);
			for (uint32 q = 0; q < NumQuads; ++q)
			{
				const uint32 Base = q * 4;
				uint32* Dst = Indices.data() + q * 6;
				Dst[0] = Base + 0; Dst[1] = Base + 1; Dst[2] = Base + 2;
				Dst[3] = Base + 0; Dst[4] = Base + 2; Dst[5] = Base + 3;
			}
			SpriteIB.EnsureCapacity(Device, Spec.IndexCount);
			SpriteIB.Update(Context, Indices.data(), Spec.IndexCount);

			Section.BufferOverride.VB       = EmitterVB->GetBuffer();
			Section.BufferOverride.VBStride = EmitterVB->GetStride();
			Section.BufferOverride.IB       = SpriteIB.GetBuffer();
		}

		MutableSections.push_back(std::move(Section));
		TotalIndexCount += Spec.IndexCount;
		++EmitterIdx;
	}

	LastIndexCount = TotalIndexCount;
	// section-level BufferOverride 사용 — OutBuffer는 빈 채로 두고 section이 자체 buffer 가짐.
	// BuildCommandForProxy가 ProxyBuffer 비어도 section 처리 가능하도록 수정됨.
	return !MutableSections.empty();
}
