#include "ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Render/Particle/ParticleVertexFactory.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"

#include <vector>

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
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;

	SpriteFactory = new FParticleSpriteVertexFactory();
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	if (SpriteFactory) { SpriteFactory->ReleaseResources(); }
	if (MeshFactory)   { MeshFactory->ReleaseResources();   }
	if (BeamFactory)   { BeamFactory->ReleaseResources();   }
	if (RibbonFactory) { RibbonFactory->ReleaseResources(); }

	delete DynamicData;   DynamicData   = nullptr;
	delete SpriteFactory; SpriteFactory = nullptr;
	delete MeshFactory;   MeshFactory   = nullptr;
	delete BeamFactory;   BeamFactory   = nullptr;
	delete RibbonFactory; RibbonFactory = nullptr;
	DynamicVB.Release();
	DynamicIB.Release();
	// ParticleMaterial은 UObjectManager가 소유 — 여기서 해제 X
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
	if (!ParticleMaterial)
	{
		ParticleMaterial = UMaterial::CreateTransient(
			ERenderPass::Translucent, EBlendState::AlphaBlend,
			EDepthStencilState::DepthReadOnly, ERasterizerState::SolidNoCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite));
	}

	// LastIndexCount는 PrepareDrawBuffer가 갱신. Section 등록만 여기서.
	SectionDraws.clear();
	if (ParticleMaterial)
	{
		SectionDraws.push_back({ ParticleMaterial, /*FirstIndex*/0, /*IndexCount*/0 });
	}
}

void FParticleSystemSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	// Particle 프록시는 정적 MeshBuffer 없음 — 매 프레임 dynamic VB/IB로 그림.
	MeshBuffer = nullptr;
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible) return;

	CachedCameraRight = Frame.CameraRight;
	CachedCameraUp    = Frame.CameraUp;
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
	OutReplay.BlendState = EBlendState::AlphaBlend;
	OutReplay.ParticleData.assign(static_cast<size_t>(OutReplay.ActiveParticleCount) * OutReplay.ParticleStride, 0);

	FBaseParticle* P = reinterpret_cast<FBaseParticle*>(OutReplay.ParticleData.data());

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

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                  FDrawCommandBuffer& OutBuffer) const
{
	if (!Device || !Context || !SpriteFactory) return false;

	// Lazy init — Shader 등 RHI 의존 리소스.
	if (!SpriteFactory->GetShader())
	{
		SpriteFactory->InitResources(Device);
	}

	// Stub 또는 실제 Replay 선택. Day 3는 stub 위주.
	FDynamicSpriteEmitterReplayData StubReplay;
	const FDynamicEmitterReplayDataBase* ReplayPtr = nullptr;
	if (DynamicData && !DynamicData->Emitters.empty())
	{
		// 첫 emitter만 시도 (Day 3 한정). Day 4+는 멀티 emitter 디스패치.
		FDynamicEmitterDataBase* EmitterData = DynamicData->Emitters[0];
		if (EmitterData && EmitterData->GetType() == EDynamicEmitterType::Sprite)
		{
			ReplayPtr = &EmitterData->GetReplayDataBase();
		}
	}
	if (!ReplayPtr)
	{
		BuildStubReplay(StubReplay, CachedWorldPos);
		ReplayPtr = &StubReplay;
	}

	// VB 채우기 — factory 내부에서 EnsureCapacity + Update(Map(DISCARD) → memcpy → Unmap).
	FParticleVertexFactory::FDrawSpec Spec;
	const bool bOk = SpriteFactory->BuildDraw(Device, Context, *ReplayPtr,
		CachedCameraRight, CachedCameraUp, DynamicVB, Spec);

	if (!bOk || Spec.VertexCount == 0 || Spec.IndexCount == 0) return false;

	// IB 생성 — quad pattern (4 verts × N quad → 6 indices × N).
	const uint32 N = Spec.IndexCount / 6;
	std::vector<uint32> Indices;
	Indices.resize(Spec.IndexCount);
	for (uint32 q = 0; q < N; ++q)
	{
		const uint32 Base = q * 4;
		uint32* Dst = Indices.data() + q * 6;
		Dst[0] = Base + 0; Dst[1] = Base + 1; Dst[2] = Base + 2;
		Dst[3] = Base + 0; Dst[4] = Base + 2; Dst[5] = Base + 3;
	}
	DynamicIB.EnsureCapacity(Device, Spec.IndexCount);
	DynamicIB.Update(Context, Indices.data(), Spec.IndexCount);

	// SectionDraws[0].IndexCount를 현재 프레임에 맞춰 갱신 (mutable cast).
	// const 메서드지만 SectionDraws는 직접 수정 불가 → const_cast로 우회.
	// (BuildCommandForProxy는 Proxy.GetSectionDraws()를 읽기 직후 호출되므로 안전.)
	auto& MutableSections = const_cast<TArray<FMeshSectionDraw>&>(GetSectionDraws());
	if (!MutableSections.empty())
	{
		MutableSections[0].IndexCount = Spec.IndexCount;
		MutableSections[0].FirstIndex = 0;
	}
	LastIndexCount = Spec.IndexCount;

	OutBuffer = {};
	OutBuffer.VB       = DynamicVB.GetBuffer();
	OutBuffer.VBStride = DynamicVB.GetStride();
	OutBuffer.IB       = DynamicIB.GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}
