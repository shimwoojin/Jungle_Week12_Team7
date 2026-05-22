#pragma once

#include "Core/Types/CoreTypes.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

// =============================================================================
// FParticleDynamicVertexBuffer
//   D3D11_USAGE_DYNAMIC + D3D11_CPU_ACCESS_WRITE 의 ring vertex buffer.
//   매 프레임 활성 입자에 대응하는 sprite/instance vertex 를 GameThread 가 채워
//   넣는다 (Map(DISCARD)/Map(NO_OVERWRITE) 패턴).
//
//   사용 흐름:
//     1) BeginAlloc(BytesNeeded, OutPtr, OutOffset)
//     2) memcpy(OutPtr, src, BytesNeeded)
//     3) EndAlloc(BytesUsed)  → draw 시 OutOffset 을 IASetVertexBuffer 의 offset 으로
//
//   Capacity 초과 시 자동 grow (다음 프레임). 한 프레임 안에서 grow 는 false 반환.
// =============================================================================
class FParticleDynamicVertexBuffer
{
public:
	FParticleDynamicVertexBuffer() = default;
	~FParticleDynamicVertexBuffer();

	bool Init(ID3D11Device* Device, uint32 InCapacityBytes, uint32 InStride);
	void Release();

	// 새 프레임 시작. ring 의 write head 를 0 으로 (필요시 grow).
	void BeginFrame(ID3D11DeviceContext* Context);

	// 사용 가능한 영역을 할당. true 면 OutPtr/OutOffset 유효.
	bool Allocate(ID3D11DeviceContext* Context, uint32 BytesNeeded,
	              void*& OutMappedPtr, uint32& OutByteOffset);

	void EndFrame(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetBuffer() const { return Buffer; }
	uint32        GetStride() const { return Stride; }

private:
	ID3D11Buffer* Buffer       = nullptr;
	uint32        CapacityBytes = 0;
	uint32        WriteHeadBytes = 0;
	uint32        Stride        = 0;
	bool          bMapped       = false;
	void*         MappedPtr     = nullptr;
};
