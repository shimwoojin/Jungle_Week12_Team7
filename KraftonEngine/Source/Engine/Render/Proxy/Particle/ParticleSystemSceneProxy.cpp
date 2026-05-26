#include "ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialManager.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Render/Particle/ParticleVertexFactory.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Profiling/Stats/Stats.h"
#include "Profiling/Stats/ParticleStats.h"

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
	// 빌보드라 카메라가 움직이면 매 프레임 빌보드 재정렬 필요.
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	// frustum 컬링 활성화: NeverCull 제거 → octree QueryFrustumAllProxies가 component의
	// dynamic WorldAABB(ComputeDynamicBounds) 기준으로 view별 컬링. bounds는 TickComponent의
	// MarkWorldBoundsDirty → UpdateActorInOctree로 매 프레임 갱신됨.
	// occlusion 컬링은 RenderCollector에서 Particle 플래그로 별도 면제(반투명 빌보드 팝핑 방지).
	ProxyFlags |= EPrimitiveProxyFlags::Particle;       // ShowFlags.bParticles 토글 + occlusion 면제 대상
	// 반투명 빌보드는 그림자 캐스터 아님 — opaque shadow depth에 쓰면 잘못된 하드 섀도우 +
	// emitter당 캐스케이드/라이트 수만큼 PrepareDrawBuffer 낭비 호출(드로우콜 4배의 원인).
	// (이전엔 NeverCull이 ShadowMapPass 캐스터 루프에서 이 제외 역할을 겸했음)
	bCastShadow = false;
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
	MeshInstanceVB.Release();
	BeamVB.Release();
	RibbonVB.Release();
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
			"Content/Material/Particle/ParticleSprite.uasset");
	}
	if (!MeshMaterial)
	{
		MeshMaterial = FMaterialManager::Get().GetOrCreateMaterial(
			"Content/Material/Particle/ParticleMesh.uasset");
	}
	if (!BeamTrailMaterial)
	{
		BeamTrailMaterial = FMaterialManager::Get().GetOrCreateMaterial(
			"Content/Material/Particle/ParticleBeamTrail.uasset");
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

			// RequiredModule.SubImagesH/V → Material cbuffer 동기화 (Cascade 패턴).
			// UMaterialInstance인 경우엔 .mat의 override 값을 우선해야 하므로 skip.
			// base Material만 RequiredModule 값에 동기화 — 자산 1개를 여러 emitter가 공유할 때
			// 마지막 set이 적용되는 한계는 그대로지만, 분리가 필요하면 instance 자산을 만들면 된다.
			if (M && Required && !Cast<UMaterialInstance>(M))
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

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                  FDrawCommandBuffer& OutBuffer) const
{
	OutBuffer = {}; // section-level BufferOverride 사용 — ProxyBuffer는 빈 상태로 둠
	if (!Device || !Context) return false;

	// ---- Replay 목록 — PSC.TickComponent → BuildDynamicData에서 채워짐.
	// Editor 모드(PIE 미실행)에선 PSC가 Tick 안 해서 DynamicData가 null이라 빈 채로 return.
	TArray<const FDynamicEmitterReplayDataBase*> Replays;
	if (DynamicData && !DynamicData->Emitters.empty())
	{
		for (FDynamicEmitterDataBase* E : DynamicData->Emitters)
		{
			if (E) Replays.push_back(&E->GetReplayDataBase());
		}
	}
	if (Replays.empty()) return false;

	// 빌보드 corner expansion + VB/IB 업로드 CPU 비용 (입자 있는 프레임만 측정).
	SCOPE_STAT_CAT("ParticlePrepareDraw", "Particles");

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
		case EDynamicEmitterType::Beam:   EmitterVB = &BeamVB;         break;
		case EDynamicEmitterType::Ribbon: EmitterVB = &RibbonVB;       break;
		default: ++EmitterIdx; continue;
		}

		// Material 먼저 결정 — sort 조건 산출 위해 BuildDraw 호출 전에 BlendState 확인.
		// RequiredModule.Material 우선 (Template 있을 때). 없으면 type별 fallback.
		UMaterial* RequiredMat = (EmitterIdx < EmitterMaterials.size())
			? EmitterMaterials[EmitterIdx] : nullptr;
		UMaterial* FallbackMat = SpriteMaterial;
		switch (Replay->EmitterType)
		{
		case EDynamicEmitterType::Mesh:   FallbackMat = MeshMaterial;      break;
		case EDynamicEmitterType::Beam:
		case EDynamicEmitterType::Ribbon: FallbackMat = BeamTrailMaterial; break;
		default: break;
		}
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
		// 정점 팩토리 타입 태깅 — ResolveSectionShader 가 emitter 종류로 파티클 셰이더 도출.
		switch (Replay->EmitterType)
		{
		case EDynamicEmitterType::Sprite: Section.VertexFactory = EVertexFactoryType::ParticleSprite; break;
		case EDynamicEmitterType::Mesh:   Section.VertexFactory = EVertexFactoryType::ParticleMesh;   break;
		case EDynamicEmitterType::Beam:   Section.VertexFactory = EVertexFactoryType::ParticleBeam;   break;
		case EDynamicEmitterType::Ribbon: Section.VertexFactory = EVertexFactoryType::ParticleRibbon; break;
		default: break;
		}
		// 입자 섹션은 자체 대표 위치로 translucent depth 정렬 (proxy 단위 정렬의 부정확 보완).
		Section.bHasSortPos  = true;
		Section.SortWorldPos = Spec.SortWorldPos;

		if (Spec.InstanceCount > 0)
		{
			// Sprite/Mesh 인스턴싱: 정적 VB(slot 0) + 정적 IB + dynamic per-instance VB(slot 1).
			// Sprite는 unit quad(4정점/6인덱스), Mesh는 실제 mesh를 slot 0에 둔다. DrawIndexedInstanced(IndexCount, N).
			Section.BufferOverride.VB               = Spec.StaticVB;
			Section.BufferOverride.VBStride         = Spec.StaticVBStride;
			Section.BufferOverride.IB               = Spec.StaticIB;
			Section.BufferOverride.InstanceCount    = Spec.InstanceCount;
			Section.BufferOverride.InstanceVB       = EmitterVB->GetBuffer();
			Section.BufferOverride.InstanceVBStride = EmitterVB->GetStride();
		}
		else
		{
			// Beam/Ribbon strip(비인스턴싱): 동적 VB(EmitterVB) + 동적 IB(factory). DrawIndexed.
			Section.BufferOverride.VB       = EmitterVB->GetBuffer();
			Section.BufferOverride.VBStride = EmitterVB->GetStride();
			Section.BufferOverride.IB       = Spec.StaticIB;
		}

		MutableSections.push_back(std::move(Section));
		TotalIndexCount += Spec.IndexCount;
		++EmitterIdx;
	}

	LastIndexCount = TotalIndexCount;

	// 제출된 파티클 섹션 수 = DrawIndexedInstanced 호출 수 (emitter당 1섹션). 멀티뷰포트면
	// 뷰당 PrepareDrawBuffer가 호출되어 자연스럽게 뷰 합산이 된다.
	PARTICLE_STATS_ADD_DRAW_CALLS(static_cast<uint32>(MutableSections.size()));

	// section-level BufferOverride 사용 — OutBuffer는 빈 채로 두고 section이 자체 buffer 가짐.
	// BuildCommandForProxy가 ProxyBuffer 비어도 section 처리 가능하도록 수정됨.
	return !MutableSections.empty();
}
