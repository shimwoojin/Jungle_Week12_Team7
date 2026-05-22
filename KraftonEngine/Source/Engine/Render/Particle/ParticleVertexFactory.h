#pragma once

#include "Core/Types/CoreTypes.h"
#include "Particle/ParticleHelper.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11InputLayout;
class FShader;
class FParticleDynamicVertexBuffer;

// =============================================================================
// FParticleVertexFactory
//   "EmitterReplayData → DX 정점/인덱스" 변환의 추상 인터페이스.
//   각 type (Sprite/Mesh/Beam/Ribbon) 별 서브클래스가 정점 layout 과
//   ConvertAndUpload 를 구현한다.
// =============================================================================
class FParticleVertexFactory
{
public:
	virtual ~FParticleVertexFactory() = default;

	virtual EDynamicEmitterType GetType() const = 0;

	// pipeline 의 입력 layout. 한 번 만들어 캐시.
	virtual ID3D11InputLayout* GetInputLayout() const = 0;
	virtual FShader*           GetShader() const = 0;

	// EmitterReplayData 의 입자 buffer 를 읽어 dynamic VB 로 변환/업로드.
	// 반환: 이 draw 의 vertex/index 카운트.
	struct FDrawSpec
	{
		uint32 VertexCount = 0;
		uint32 IndexCount  = 0;
		uint32 VertexByteOffset = 0;
		uint32 IndexByteOffset  = 0;
	};
	virtual bool BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
	                       const FDynamicEmitterReplayDataBase& Replay,
	                       FParticleDynamicVertexBuffer& InOutVB,
	                       FDrawSpec& OutDraw) = 0;
};

// -----------------------------------------------------------------------------
// FParticleSpriteVertexFactory
//   FParticleSpriteVertex (Position/Color/Size/Rotation/UV/SubImage) 를 만든다.
//   ScreenAlignment / Sort 적용은 SceneProxy 가 ReplayData 채우는 단계에서.
// -----------------------------------------------------------------------------
class FParticleSpriteVertexFactory : public FParticleVertexFactory
{
public:
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Sprite; }
	ID3D11InputLayout*  GetInputLayout() const override { return InputLayout; }
	FShader*            GetShader()      const override { return Shader; }

	bool BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
	               const FDynamicEmitterReplayDataBase& Replay,
	               FParticleDynamicVertexBuffer& InOutVB,
	               FDrawSpec& OutDraw) override;

	void InitResources(ID3D11Device* Device);
	void ReleaseResources();

protected:
	ID3D11InputLayout* InputLayout = nullptr;
	FShader*           Shader      = nullptr;
};

// -----------------------------------------------------------------------------
// FParticleMeshVertexFactory
//   StaticMesh 의 vertex/index 를 그대로 쓰고, per-instance buffer 에
//   transform/color/sub-image 를 흘려넣는다 (instanced draw).
// -----------------------------------------------------------------------------
class FParticleMeshVertexFactory : public FParticleVertexFactory
{
public:
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Mesh; }
	ID3D11InputLayout*  GetInputLayout() const override { return InputLayout; }
	FShader*            GetShader()      const override { return Shader; }

	bool BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
	               const FDynamicEmitterReplayDataBase& Replay,
	               FParticleDynamicVertexBuffer& InOutVB,
	               FDrawSpec& OutDraw) override;

	void InitResources(ID3D11Device* Device);
	void ReleaseResources();

protected:
	ID3D11InputLayout* InputLayout = nullptr;
	FShader*           Shader      = nullptr;
};

// -----------------------------------------------------------------------------
// FParticleBeamVertexFactory / FParticleRibbonVertexFactory
//   Beam: source→target 사이 tessellation. Ribbon: 시간순 sample 자취.
//   각자 본인의 dynamic vertex layout 을 가진다.
// -----------------------------------------------------------------------------
class FParticleBeamVertexFactory : public FParticleVertexFactory
{
public:
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Beam; }
	ID3D11InputLayout*  GetInputLayout() const override { return InputLayout; }
	FShader*            GetShader()      const override { return Shader; }

	bool BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
	               const FDynamicEmitterReplayDataBase& Replay,
	               FParticleDynamicVertexBuffer& InOutVB,
	               FDrawSpec& OutDraw) override;

	void InitResources(ID3D11Device* Device);
	void ReleaseResources();

protected:
	ID3D11InputLayout* InputLayout = nullptr;
	FShader*           Shader      = nullptr;
};

class FParticleRibbonVertexFactory : public FParticleVertexFactory
{
public:
	EDynamicEmitterType GetType() const override { return EDynamicEmitterType::Ribbon; }
	ID3D11InputLayout*  GetInputLayout() const override { return InputLayout; }
	FShader*            GetShader()      const override { return Shader; }

	bool BuildDraw(ID3D11Device* Device, ID3D11DeviceContext* Context,
	               const FDynamicEmitterReplayDataBase& Replay,
	               FParticleDynamicVertexBuffer& InOutVB,
	               FDrawSpec& OutDraw) override;

	void InitResources(ID3D11Device* Device);
	void ReleaseResources();

protected:
	ID3D11InputLayout* InputLayout = nullptr;
	FShader*           Shader      = nullptr;
};
