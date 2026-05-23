#include "ParticleVertexFactory.h"

#include "Render/Particle/ParticleDynamicVertexBuffer.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/VertexTypes.h"

#include <d3d11.h>

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
	InputLayout = nullptr;
}

bool FParticleSpriteVertexFactory::BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
                                             const FDynamicEmitterReplayDataBase& Replay,
                                             const FVector& CameraRight, const FVector& CameraUp,
                                             FParticleDynamicVertexBuffer& InOutVB,
                                             FDrawSpec& OutDraw)
{
	OutDraw = {};
	const uint32 N = Replay.ActiveParticleCount;
	if (N == 0 || Replay.ParticleStride == 0) return false;

	const uint32 VertCount  = N * 4;
	const uint32 IndexCount = N * 6;
	const uint32 BytesNeeded = VertCount * sizeof(FVertexPNCT);

	// VB 용량 확보 — capacity 부족 시 재할당 (한 프레임 grow 비용 1회)
	if (InOutVB.GetStride() == 0 || !InOutVB.GetBuffer())
	{
		const uint32 InitialCap = BytesNeeded > 4096 ? BytesNeeded : 4096;
		if (!InOutVB.Init(Device, InitialCap, sizeof(FVertexPNCT))) return false;
		InOutVB.BeginFrame(Context);
	}

	void* MappedPtr = nullptr;
	uint32 ByteOffset = 0;
	if (!InOutVB.Allocate(Context, BytesNeeded, MappedPtr, ByteOffset))
	{
		// capacity 부족 → grow 후 재시도
		InOutVB.EndFrame(Context);
		const uint32 NewCap = BytesNeeded * 2;
		if (!InOutVB.Init(Device, NewCap, sizeof(FVertexPNCT))) return false;
		InOutVB.BeginFrame(Context);
		if (!InOutVB.Allocate(Context, BytesNeeded, MappedPtr, ByteOffset)) return false;
	}

	FVertexPNCT* Dst = static_cast<FVertexPNCT*>(MappedPtr);

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
			Dst[i * 4 + c].Position = Corner;
			Dst[i * 4 + c].Normal   = { 0, 0, 0 };
			Dst[i * 4 + c].Color    = P.Color;
			Dst[i * 4 + c].UV       = { CornerUV[c][0], CornerUV[c][1] };
		}
	}

	OutDraw.VertexCount      = VertCount;
	OutDraw.IndexCount       = IndexCount;
	OutDraw.VertexByteOffset = ByteOffset;
	OutDraw.IndexByteOffset  = 0;
	return true;
}

// =============================================================================
// Mesh / Beam / Ribbon — Day 3에서는 미구현 (Day 5에서 Mesh, 이후 Beam/Ribbon)
// =============================================================================
void FParticleMeshVertexFactory::InitResources(ID3D11Device* /*Device*/) {}
void FParticleMeshVertexFactory::ReleaseResources() {}
bool FParticleMeshVertexFactory::BuildDraw(ID3D11Device* /*Device*/, ID3D11DeviceContext* /*Context*/,
                                           const FDynamicEmitterReplayDataBase& /*Replay*/,
                                           const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
                                           FParticleDynamicVertexBuffer& /*InOutVB*/,
                                           FDrawSpec& OutDraw)
{
	OutDraw = {};
	return false; // Day 5에서 구현
}

void FParticleBeamVertexFactory::InitResources(ID3D11Device* /*Device*/) {}
void FParticleBeamVertexFactory::ReleaseResources() {}
bool FParticleBeamVertexFactory::BuildDraw(ID3D11Device* /*Device*/, ID3D11DeviceContext* /*Context*/,
                                           const FDynamicEmitterReplayDataBase& /*Replay*/,
                                           const FVector& /*CameraRight*/, const FVector& /*CameraUp*/,
                                           FParticleDynamicVertexBuffer& /*InOutVB*/,
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
                                             FParticleDynamicVertexBuffer& /*InOutVB*/,
                                             FDrawSpec& OutDraw)
{
	OutDraw = {};
	return false;
}
