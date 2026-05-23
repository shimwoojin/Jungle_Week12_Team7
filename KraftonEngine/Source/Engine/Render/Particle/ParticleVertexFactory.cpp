#include "ParticleVertexFactory.h"

#include "Render/Resource/Buffer.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/MeshManager.h"

#include <d3d11.h>
#include <vector>
#include <algorithm>
#include <cmath>

static bool ShouldSortParticleIndices(EParticleReplaySortMode SortMode,
                                      const FBaseParticle& A, const FVector& AWorldPos,
                                      const FBaseParticle& B, const FVector& BWorldPos,
                                      const FVector& CameraPosition)
{
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
void FParticleSpriteVertexFactory::InitResources(ID3D11Device* /*Device*/)
{
	Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite);
}

void FParticleSpriteVertexFactory::ReleaseResources()
{
	Shader = nullptr;
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

	const uint32 VertCount = N * 4;
	const uint32 IndexCount = N * 6;

	const auto& SpriteReplay = static_cast<const FDynamicSpriteEmitterReplayData&>(Replay);
	const int32 SubH = SpriteReplay.SubImagesHorizontal > 0 ? SpriteReplay.SubImagesHorizontal : 1;
	const int32 SubV = SpriteReplay.SubImagesVertical > 0 ? SpriteReplay.SubImagesVertical : 1;
	const float TileW = 1.0f / static_cast<float>(SubH);
	const float TileH = 1.0f / static_cast<float>(SubV);

	std::vector<FVertexPNCT> Vertices(VertCount);

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

	static const float CornerSign[4][2] = {
		{ -0.5f,  0.5f },
		{  0.5f,  0.5f },
		{  0.5f, -0.5f },
		{ -0.5f, -0.5f },
	};
	static const float CornerUV[4][2] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
	};

	for (uint32 sortI = 0; sortI < N; ++sortI)
	{
		const uint32 i = SortedIdx[sortI];
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Stride);
		const float HalfW = P.Size.X * 0.5f;
		const float HalfH = P.Size.Y * 0.5f;
		const FVector WorldCenter = GetWorldPos(i);

		const int32 RawIdx = P.SubImageIndex >= 0 ? P.SubImageIndex : 0;
		const int32 Col = RawIdx % SubH;
		const int32 Row = (RawIdx / SubH) % SubV;

		for (int c = 0; c < 4; ++c)
		{
			const float Dx = CornerSign[c][0] * (HalfW * 2.0f);
			const float Dy = CornerSign[c][1] * (HalfH * 2.0f);
			FVector Corner = WorldCenter + CameraRight * Dx + CameraUp * Dy;
			const float U = (static_cast<float>(Col) + CornerUV[c][0]) * TileW;
			const float V = (static_cast<float>(Row) + CornerUV[c][1]) * TileH;
			Vertices[sortI * 4 + c].Position = Corner;
			Vertices[sortI * 4 + c].Normal = { 0, 0, 0 };
			Vertices[sortI * 4 + c].Color = P.Color;
			Vertices[sortI * 4 + c].UV = { U, V };
		}
	}

	if (InOutVB.GetStride() == 0 || !InOutVB.GetBuffer())
	{
		InOutVB.Create(Device, VertCount, sizeof(FVertexPNCT));
	}
	else
	{
		InOutVB.EnsureCapacity(Device, VertCount);
	}
	if (!InOutVB.Update(Context, Vertices.data(), VertCount)) return false;

	OutDraw.VertexCount = VertCount;
	OutDraw.IndexCount = IndexCount;
	OutDraw.VertexByteOffset = 0;
	OutDraw.IndexByteOffset = 0;
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
		V.SubImageIndex = P.SubImageIndex;
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
