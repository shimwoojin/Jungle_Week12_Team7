#include "ParticleDynamicVertexBuffer.h"

FParticleDynamicVertexBuffer::~FParticleDynamicVertexBuffer()
{
	Release();
}

bool FParticleDynamicVertexBuffer::Init(ID3D11Device* Device, uint32 InCapacityBytes, uint32 InStride)
{
	CapacityBytes = InCapacityBytes;
	Stride        = InStride;
	// TODO: D3D11_USAGE_DYNAMIC + CPU_ACCESS_WRITE 로 ID3D11Buffer 생성.
	return false;
}

void FParticleDynamicVertexBuffer::Release()
{
	// TODO: Buffer->Release().
	Buffer        = nullptr;
	CapacityBytes = 0;
	Stride        = 0;
}

void FParticleDynamicVertexBuffer::BeginFrame(ID3D11DeviceContext* Context)
{
	WriteHeadBytes = 0;
	bMapped        = false;
	// TODO: 첫 Allocate 에서 Map(DISCARD) 로 시작.
}

bool FParticleDynamicVertexBuffer::Allocate(ID3D11DeviceContext* Context, uint32 BytesNeeded,
                                            void*& OutMappedPtr, uint32& OutByteOffset)
{
	if (WriteHeadBytes + BytesNeeded > CapacityBytes) return false;
	// TODO: 처음이면 Map(DISCARD), 이후엔 Map(NO_OVERWRITE).
	OutMappedPtr  = nullptr;
	OutByteOffset = WriteHeadBytes;
	WriteHeadBytes += BytesNeeded;
	return false;
}

void FParticleDynamicVertexBuffer::EndFrame(ID3D11DeviceContext* Context)
{
	if (bMapped) {
		// TODO: Context->Unmap(Buffer, 0).
		bMapped = false;
	}
}
