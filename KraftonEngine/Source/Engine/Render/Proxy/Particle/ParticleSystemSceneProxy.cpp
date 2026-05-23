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

	// Factories는 GetOrCreateFactory가 lazy 생성. 여기서는 빈 배열.
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	for (FParticleVertexFactory*& F : Factories)
	{
		if (F) { F->ReleaseResources(); delete F; F = nullptr; }
	}
	delete DynamicData; DynamicData = nullptr;
	DynamicVB.Release();
	DynamicIB.Release();
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
	if (!Device || !Context) return false;

	// ---- Replay 목록 결정 (실제 DynamicData 우선, 없으면 stub) ----
	FDynamicSpriteEmitterReplayData StubReplay;
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
		BuildStubReplay(StubReplay, CachedWorldPos);
		Replays.push_back(&StubReplay);
	}

	// ---- 타입별 디스패치 ----
	// Day 3 한정: 단일 SectionDraw + 첫 성공 emitter만 표시.
	// Day 4+: 멀티 emitter면 emitter별 SectionDraw로 분리 + VB scratch accumulator 필요.
	FParticleVertexFactory::FDrawSpec Spec;
	bool bAnyDrawn = false;
	for (const FDynamicEmitterReplayDataBase* Replay : Replays)
	{
		FParticleVertexFactory* Factory = GetOrCreateFactory(Replay->EmitterType, Device);
		if (!Factory) continue;
		if (Factory->BuildDraw(Device, Context, *Replay,
			CachedCameraRight, CachedCameraUp, DynamicVB, Spec))
		{
			bAnyDrawn = true;
			break; // 첫 성공 emitter만 그림 (다음 emitter가 VB를 overwrite하지 않도록)
		}
	}
	if (!bAnyDrawn || Spec.VertexCount == 0 || Spec.IndexCount == 0) return false;

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
