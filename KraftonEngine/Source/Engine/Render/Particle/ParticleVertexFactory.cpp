#include "ParticleVertexFactory.h"

#include "Render/Resource/Buffer.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/MeshManager.h"

#include <d3d11.h>
#include <vector>

// =============================================================================
// Sprite — CPU 빌보드 expansion: 입자 1개 → 4 corner 정점 (FVertexPNCT)
// =============================================================================
void FParticleSpriteVertexFactory::InitResources(ID3D11Device* /*Device*/)
{
	// FShader 가 reflection 으로 InputLayout 자동 생성 — 별도 layout 작업 불필요.
	Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite);
}

void FParticleSpriteVertexFactory::ReleaseResources()
{
	// FShaderManager가 owning. 포인터만 nullify.
	Shader = nullptr;
}

bool FParticleSpriteVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                             const FDynamicEmitterReplayDataBase& Replay,
                                             const FVector& CameraRight, const FVector& CameraUp,
                                             FDynamicVertexBuffer& InOutVB,
                                             FDrawSpec& OutDraw)
{
	OutDraw = {};
	const uint32 N = Replay.ActiveParticleCount;
	if (N == 0 || Replay.ParticleStride == 0) return false;

	const uint32 VertCount  = N * 4;
	const uint32 IndexCount = N * 6;

	// CPU 측 임시 버퍼에 모두 채운 뒤 한 번에 GPU 업로드.
	std::vector<FVertexPNCT> Vertices(VertCount);

	// 입자 데이터는 [ActiveCount × Stride] 의 raw 바이트. FBaseParticle 헤더로 캐스팅.
	const uint8* RawBase = Replay.ParticleData.data();

	// 4 corner 정의 (UV + Right/Up 오프셋 부호)
	static const float CornerSign[4][2] = {
		{ -0.5f,  0.5f }, // TL
		{  0.5f,  0.5f }, // TR
		{  0.5f, -0.5f }, // BR
		{ -0.5f, -0.5f }, // BL
	};
	static const float CornerUV[4][2] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
	};

	for (uint32 i = 0; i < N; ++i)
	{
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Replay.ParticleStride);
		const float HalfW = P.Size.X * 0.5f;
		const float HalfH = P.Size.Y * 0.5f;
		// LocalToWorld 처리 — bUseLocalSpace 시 Location을 LocalToWorld로 변환
		FVector WorldCenter = P.Location;
		if (Replay.bUseLocalSpace)
		{
			WorldCenter = Replay.LocalToWorld.TransformPositionWithW(WorldCenter);
		}

		for (int c = 0; c < 4; ++c)
		{
			const float Dx = CornerSign[c][0] * (HalfW * 2.0f); // CornerSign이 ±0.5라 *2 보정
			const float Dy = CornerSign[c][1] * (HalfH * 2.0f);
			FVector Corner = WorldCenter + CameraRight * Dx + CameraUp * Dy;
			Vertices[i * 4 + c].Position = Corner;
			Vertices[i * 4 + c].Normal   = { 0, 0, 0 };
			Vertices[i * 4 + c].Color    = P.Color;
			Vertices[i * 4 + c].UV       = { CornerUV[c][0], CornerUV[c][1] };
		}
	}

	// VB 용량 확보 + 업로드. Stride가 0이면 첫 호출이라 Create 먼저.
	if (InOutVB.GetStride() == 0 || !InOutVB.GetBuffer())
	{
		InOutVB.Create(Device, VertCount, sizeof(FVertexPNCT));
	}
	else
	{
		InOutVB.EnsureCapacity(Device, VertCount);
	}
	if (!InOutVB.Update(Context, Vertices.data(), VertCount)) return false;

	OutDraw.VertexCount      = VertCount;
	OutDraw.IndexCount       = IndexCount;
	OutDraw.VertexByteOffset = 0;
	OutDraw.IndexByteOffset  = 0;
	return true;
}

// =============================================================================
// Mesh — 정적 StaticMesh + per-instance stream으로 DrawIndexedInstanced
// =============================================================================
void FParticleMeshVertexFactory::InitResources(ID3D11Device* Device)
{
	Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleMesh);

	// Replay.Mesh가 nullptr인 stub용 fallback — Content의 기본 Cube 자산.
	// Mesh primitive(FVertex 포맷) 대신 UStaticMesh(FVertexPNCT)라 셰이더 input layout과 매칭.
	if (!CachedCubeFallback && Device)
	{
		CachedCubeFallback = FMeshManager::LoadStaticMesh("Content/Data/BasicShape/Cube.OBJ", Device);
	}
}

void FParticleMeshVertexFactory::ReleaseResources()
{
	Shader = nullptr;
	CachedCubeFallback = nullptr; // UObjectManager 소유 — pointer만 비움
}

bool FParticleMeshVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                           const FDynamicEmitterReplayDataBase& Replay,
                                           const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
                                           FDynamicVertexBuffer& InOutVB,
                                           FDrawSpec& OutDraw)
{
	OutDraw = {};
	const uint32 N = Replay.ActiveParticleCount;
	if (N == 0 || Replay.ParticleStride == 0) return false;

	// Mesh emitter 전용 데이터로 캐스팅 (caller가 type 보장).
	const auto& MeshReplay = static_cast<const FDynamicMeshEmitterReplayData&>(Replay);
	// Mesh null이면 InitResources에서 로드한 Content/Data/BasicShape/Cube.OBJ fallback.
	UStaticMesh* Mesh = MeshReplay.Mesh ? MeshReplay.Mesh : CachedCubeFallback;
	if (!Mesh) return false;
	FMeshBuffer* MB = Mesh->GetLODMeshBuffer(0);
	if (!MB || !MB->IsValid()) return false;

	// per-instance 정점 채우기.
	std::vector<FParticleMeshInstanceVertex> Instances(N);
	const uint8* RawBase = Replay.ParticleData.data();

	for (uint32 i = 0; i < N; ++i)
	{
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(RawBase + i * Replay.ParticleStride);

		// world matrix = Scale × RotationZ × Translation (row-major, mul(v, M))
		FMatrix M = FMatrix::MakeScaleMatrix(P.Size)
		          * FMatrix::MakeRotationZ(P.Rotation)
		          * FMatrix::MakeTranslationMatrix(P.Location);
		if (Replay.bUseLocalSpace)
		{
			M = M * Replay.LocalToWorld;
		}

		FParticleMeshInstanceVertex& V = Instances[i];
		V.Transform0 = FVector4{ M.M[0][0], M.M[0][1], M.M[0][2], M.M[0][3] };
		V.Transform1 = FVector4{ M.M[1][0], M.M[1][1], M.M[1][2], M.M[1][3] };
		V.Transform2 = FVector4{ M.M[2][0], M.M[2][1], M.M[2][2], M.M[2][3] };
		V.Transform3 = FVector4{ M.M[3][0], M.M[3][1], M.M[3][2], M.M[3][3] };
		V.Color = P.Color;
		V.SubImageIndex = P.SubImageIndex;
	}

	// Dynamic instance VB upload.
	if (InOutVB.GetStride() == 0 || !InOutVB.GetBuffer())
	{
		InOutVB.Create(Device, N, sizeof(FParticleMeshInstanceVertex));
	}
	else
	{
		InOutVB.EnsureCapacity(Device, N);
	}
	if (!InOutVB.Update(Context, Instances.data(), N)) return false;

	// 정적 mesh 자원 + instance count를 OutDraw로 전달.
	OutDraw.StaticVB       = MB->GetVertexBuffer().GetBuffer();
	OutDraw.StaticVBStride = MB->GetVertexBuffer().GetStride();
	OutDraw.StaticIB       = MB->GetIndexBuffer().GetBuffer();
	OutDraw.VertexCount    = MB->GetVertexBuffer().GetVertexCount();
	OutDraw.IndexCount     = MB->GetIndexBuffer().GetIndexCount();
	OutDraw.InstanceCount  = N;
	return true;
}

void FParticleBeamVertexFactory::InitResources(ID3D11Device* /*Device*/) {}
void FParticleBeamVertexFactory::ReleaseResources() {}
bool FParticleBeamVertexFactory::BuildDraw(ID3D11Device* /*Device*/, ID3D11DeviceContext* /*Context*/,
                                           const FDynamicEmitterReplayDataBase& /*Replay*/,
                                           const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
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
                                             FDynamicVertexBuffer& /*InOutVB*/,
                                             FDrawSpec& OutDraw)
{
	OutDraw = {};
	return false;
}
