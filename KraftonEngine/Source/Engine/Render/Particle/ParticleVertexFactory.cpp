#include "ParticleVertexFactory.h"

#include "Render/Resource/Buffer.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/MeshManager.h"
#include "Materials/Material.h"

#include <d3d11.h>
#include <vector>
#include <algorithm>
#include <cmath>

static bool ShouldSortParticleIndices(EParticleReplaySortMode SortMode,
                                      const FBaseParticle& A, const FVector& AWorldPos,
                                      const FBaseParticle& B, const FVector& BWorldPos,
                                      const FVector& CameraPosition)
{
	// Replay.SortMode가 지정한 "입자 내부 정렬 정책"을 함수 하나로 모은다.
	// SceneProxy는 정렬 필요 여부만 결정하고, 실제 comparator 선택은 factory가 맡는다.
	switch (SortMode)
	{
	case EParticleReplaySortMode::Age_OldestFirst:
		return A.RelativeTime > B.RelativeTime;
	case EParticleReplaySortMode::Age_NewestFirst:
		return A.RelativeTime < B.RelativeTime;
	case EParticleReplaySortMode::ViewProjDepth:
		// Approximate projected depth with camera distance until a camera-forward/depth input is added.
	case EParticleReplaySortMode::ViewDistance:
	{
		const FVector ToA = AWorldPos - CameraPosition;
		const FVector ToB = BWorldPos - CameraPosition;
		return ToA.Dot(ToA) > ToB.Dot(ToB);
	}
	case EParticleReplaySortMode::None:
	default:
		return false;
	}
}

// =============================================================================
// Sprite CPU billboard expansion
// =============================================================================
void FParticleSpriteVertexFactory::InitResources(ID3D11Device* Device)
{
	Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite);

	// 정적 unit quad — corner 부호(±0.5) + tile 내부 base UV. 모든 입자가 공유한다.
	if (Device && !QuadVB.GetBuffer())
	{
		const FParticleSpriteQuadVertex QuadVerts[4] = {
			{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f } },
			{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f } },
			{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		};
		const uint32 QuadIndices[6] = { 0, 1, 2, 0, 2, 3 };
		QuadVB.Create(Device, QuadVerts, 4, sizeof(QuadVerts), sizeof(FParticleSpriteQuadVertex));
		QuadIB.Create(Device, QuadIndices, 6, sizeof(QuadIndices));
	}
}

void FParticleSpriteVertexFactory::ReleaseResources()
{
	Shader = nullptr;
	QuadVB.Release();
	QuadIB.Release();
}

bool FParticleSpriteVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                             const FDynamicEmitterReplayDataBase& Replay,
                                             const FVector& CameraRight, const FVector& CameraUp,
                                             const FVector& CameraPosition,
                                             bool bRequiresSort,
                                             EParticleReplaySortMode SortMode,
                                             FDynamicVertexBuffer& InOutVB,
                                             FDrawSpec& OutDraw)
{
	OutDraw = {};
	const FParticleDataView View = Replay.GetParticleView();
	const uint32 N = View.ActiveParticleCount;
	if (N == 0 || View.ParticleStride == 0 || !View.ParticleData) return false;
	if (!QuadVB.GetBuffer() || !QuadIB.GetBuffer()) return false; // unit quad 미초기화

	const auto& SpriteReplay = static_cast<const FDynamicSpriteEmitterReplayData&>(Replay);
	const int32 SubH = SpriteReplay.SubImagesHorizontal > 0 ? SpriteReplay.SubImagesHorizontal : 1;
	const int32 SubV = SpriteReplay.SubImagesVertical > 0 ? SpriteReplay.SubImagesVertical : 1;
	const int32 FrameCount = SubH * SubV;
	const int32 Alignment = static_cast<int32>(SpriteReplay.Alignment);

	const uint8* RawBase = View.ParticleData;
	const uint32 Stride = View.ParticleStride;
	const bool bLocal = Replay.bUseLocalSpace;
	const FMatrix& L2W = Replay.LocalToWorld;

	auto GetWorldPos = [RawBase, Stride, bLocal, &L2W](uint32 i) -> FVector
	{
		const auto& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Stride);
		FVector W = P.Location;
		if (bLocal) W = L2W.TransformPositionWithW(W);
		return W;
	};

	// Sort: instance 적재 순서가 곧 draw 순서이므로 정렬된 index 순으로 채운다.
	std::vector<uint32> SortedIdx(N);
	for (uint32 i = 0; i < N; ++i) SortedIdx[i] = i;
	if (bRequiresSort)
	{
		std::sort(SortedIdx.begin(), SortedIdx.end(),
			[&GetWorldPos, &CameraPosition, RawBase, Stride, SortMode](uint32 a, uint32 b)
			{
				const FBaseParticle& ParticleA =
					*reinterpret_cast<const FBaseParticle*>(RawBase + a * Stride);
				const FBaseParticle& ParticleB =
					*reinterpret_cast<const FBaseParticle*>(RawBase + b * Stride);
				return ShouldSortParticleIndices(
					SortMode,
					ParticleA, GetWorldPos(a),
					ParticleB, GetWorldPos(b),
					CameraPosition);
			});
	}

	// 입자 1개 = 인스턴스 1개. 빌보드 corner expansion / atlas UV / alignment는 VS가 GPU에서 처리.
	std::vector<FParticleSpriteInstanceVertex> Instances(N);
	for (uint32 sortI = 0; sortI < N; ++sortI)
	{
		const uint32 i = SortedIdx[sortI];
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Stride);

		FParticleSpriteInstanceVertex& V = Instances[sortI];
		V.Center   = GetWorldPos(i);
		V.Velocity = bLocal ? L2W.TransformVector(P.Velocity) : P.Velocity;
		V.Size     = FVector2{ P.Size.X, P.Size.Y };
		V.Rotation = P.Rotation;
		V.Color    = P.Color;

		// SubUV: SubImageIndex 미설정(<=0)이고 atlas가 여러 frame이면 lifetime 비례 fallback.
		int32 RawIdx = P.SubImageIndex >= 0 ? P.SubImageIndex : 0;
		if (RawIdx <= 0 && FrameCount > 1)
		{
			RawIdx = static_cast<int32>(P.RelativeTime * static_cast<float>(FrameCount));
			if (RawIdx < 0) RawIdx = 0;
		}
		V.SubImageIndex = RawIdx;
		V.Alignment = Alignment;
	}

	if (InOutVB.GetStride() == 0 || !InOutVB.GetBuffer())
	{
		InOutVB.Create(Device, N, sizeof(FParticleSpriteInstanceVertex));
	}
	else
	{
		InOutVB.EnsureCapacity(Device, N);
	}
	if (!InOutVB.Update(Context, Instances.data(), N)) return false;

	// slot 0 = unit quad(정적), slot 1 = per-instance(InOutVB). DrawIndexedInstanced(6, N).
	OutDraw.StaticVB       = QuadVB.GetBuffer();
	OutDraw.StaticVBStride = QuadVB.GetStride();
	OutDraw.StaticIB       = QuadIB.GetBuffer();
	OutDraw.VertexCount    = QuadVB.GetVertexCount();
	OutDraw.IndexCount     = QuadIB.GetIndexCount();
	OutDraw.InstanceCount  = N;
	return true;
}

// =============================================================================
// Mesh instanced path
// =============================================================================
void FParticleMeshVertexFactory::InitResources(ID3D11Device* Device)
{
	Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleMesh);

	if (!CachedCubeFallback && Device)
	{
		CachedCubeFallback = FMeshManager::LoadStaticMesh("Content/Data/BasicShape/Cube.OBJ", Device);
	}
}

void FParticleMeshVertexFactory::ReleaseResources()
{
	Shader = nullptr;
	CachedCubeFallback = nullptr;
}

bool FParticleMeshVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                           const FDynamicEmitterReplayDataBase& Replay,
                                           const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
                                           const FVector& CameraPosition,
                                           bool bRequiresSort,
                                           EParticleReplaySortMode SortMode,
                                           FDynamicVertexBuffer& InOutVB,
                                           FDrawSpec& OutDraw)
{
	OutDraw = {};
	const FParticleDataView View = Replay.GetParticleView();
	const uint32 N = View.ActiveParticleCount;
	if (N == 0 || View.ParticleStride == 0 || !View.ParticleData) return false;

	const auto& MeshReplay = static_cast<const FDynamicMeshEmitterReplayData&>(Replay);
	UStaticMesh* Mesh = MeshReplay.Mesh ? MeshReplay.Mesh : CachedCubeFallback;
	if (!Mesh) return false;
	FMeshBuffer* MB = Mesh->GetLODMeshBuffer(0);
	if (!MB || !MB->IsValid()) return false;

	const uint8* RawBase = View.ParticleData;
	const uint32 Stride = View.ParticleStride;

	int32 SubH = 1;
	int32 SubV = 1;
	if (MeshReplay.Material)
	{
		float fH = 1.0f;
		float fV = 1.0f;
		MeshReplay.Material->GetScalarParameter("SubImagesH", fH);
		MeshReplay.Material->GetScalarParameter("SubImagesV", fV);
		if (fH >= 1.0f) SubH = static_cast<int32>(fH);
		if (fV >= 1.0f) SubV = static_cast<int32>(fV);
	}
	const int32 FrameCount = SubH * SubV;

	auto GetWorldPos = [RawBase, Stride, &Replay](uint32 i) -> FVector
	{
		const auto& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Stride);
		FVector W = P.Location;
		if (Replay.bUseLocalSpace)
		{
			W = Replay.LocalToWorld.TransformPositionWithW(W);
		}
		return W;
	};

	std::vector<uint32> SortedIdx(N);
	for (uint32 i = 0; i < N; ++i) SortedIdx[i] = i;
	if (bRequiresSort)
	{
		std::sort(SortedIdx.begin(), SortedIdx.end(),
			[&GetWorldPos, &CameraPosition, RawBase, Stride, SortMode](uint32 a, uint32 b)
			{
				const FBaseParticle& ParticleA =
					*reinterpret_cast<const FBaseParticle*>(RawBase + a * Stride);
				const FBaseParticle& ParticleB =
					*reinterpret_cast<const FBaseParticle*>(RawBase + b * Stride);
				return ShouldSortParticleIndices(
					SortMode,
					ParticleA, GetWorldPos(a),
					ParticleB, GetWorldPos(b),
					CameraPosition);
			});
	}

	std::vector<FParticleMeshInstanceVertex> Instances(N);
	for (uint32 sortI = 0; sortI < N; ++sortI)
	{
		const uint32 i = SortedIdx[sortI];
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Stride);

		FMatrix M = FMatrix::MakeScaleMatrix(P.Size)
			* FMatrix::MakeRotationZ(P.Rotation)
			* FMatrix::MakeTranslationMatrix(P.Location);
		if (Replay.bUseLocalSpace)
		{
			M = M * Replay.LocalToWorld;
		}

		FParticleMeshInstanceVertex& V = Instances[sortI];
		V.Transform0 = FVector4{ M.M[0][0], M.M[0][1], M.M[0][2], M.M[0][3] };
		V.Transform1 = FVector4{ M.M[1][0], M.M[1][1], M.M[1][2], M.M[1][3] };
		V.Transform2 = FVector4{ M.M[2][0], M.M[2][1], M.M[2][2], M.M[2][3] };
		V.Transform3 = FVector4{ M.M[3][0], M.M[3][1], M.M[3][2], M.M[3][3] };
		V.Color = P.Color;

		int32 SubIdx = P.SubImageIndex >= 0 ? P.SubImageIndex : 0;
		if (SubIdx <= 0 && FrameCount > 1)
		{
			SubIdx = static_cast<int32>(P.RelativeTime * static_cast<float>(FrameCount));
			if (SubIdx < 0) SubIdx = 0;
		}
		V.SubImageIndex = SubIdx;
	}

	if (InOutVB.GetStride() == 0 || !InOutVB.GetBuffer())
	{
		InOutVB.Create(Device, N, sizeof(FParticleMeshInstanceVertex));
	}
	else
	{
		InOutVB.EnsureCapacity(Device, N);
	}
	if (!InOutVB.Update(Context, Instances.data(), N)) return false;

	OutDraw.StaticVB = MB->GetVertexBuffer().GetBuffer();
	OutDraw.StaticVBStride = MB->GetVertexBuffer().GetStride();
	OutDraw.StaticIB = MB->GetIndexBuffer().GetBuffer();
	OutDraw.VertexCount = MB->GetVertexBuffer().GetVertexCount();
	OutDraw.IndexCount = MB->GetIndexBuffer().GetIndexCount();
	OutDraw.InstanceCount = N;
	return true;
}

void FParticleBeamVertexFactory::InitResources(ID3D11Device* /*Device*/) {}
void FParticleBeamVertexFactory::ReleaseResources() {}
bool FParticleBeamVertexFactory::BuildDraw(ID3D11Device* /*Device*/, ID3D11DeviceContext* /*Context*/,
                                           const FDynamicEmitterReplayDataBase& /*Replay*/,
                                           const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
                                           const FVector& /*CameraPosition*/,
                                           bool /*bRequiresSort*/,
                                           EParticleReplaySortMode /*SortMode*/,
                                           FDynamicVertexBuffer& /*InOutVB*/,
                                           FDrawSpec& OutDraw)
{
	OutDraw = {};
	return false;
}

void FParticleRibbonVertexFactory::InitResources(ID3D11Device* /*Device*/) {}
void FParticleRibbonVertexFactory::ReleaseResources() {}
bool FParticleRibbonVertexFactory::BuildDraw(ID3D11Device* /*Device*/, ID3D11DeviceContext* /*Context*/,
                                             const FDynamicEmitterReplayDataBase& /*Replay*/,
                                             const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
                                             const FVector& /*CameraPosition*/,
                                             bool /*bRequiresSort*/,
                                             EParticleReplaySortMode /*SortMode*/,
                                             FDynamicVertexBuffer& /*InOutVB*/,
                                             FDrawSpec& OutDraw)
{
	OutDraw = {};
	return false;
}
