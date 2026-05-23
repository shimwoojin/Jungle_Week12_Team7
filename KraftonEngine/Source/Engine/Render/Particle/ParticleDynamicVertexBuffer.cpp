#include "ParticleDynamicVertexBuffer.h"

#include <d3d11.h>

FParticleDynamicVertexBuffer::~FParticleDynamicVertexBuffer()
{
	Release();
}

bool FParticleDynamicVertexBuffer::Init(ID3D11Device* Device, uint32 InCapacityBytes, uint32 InStride)
{
	Release();
	CapacityBytes = InCapacityBytes;
	Stride        = InStride;
	if (!Device || CapacityBytes == 0 || Stride == 0) return false;

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth      = CapacityBytes;
	Desc.Usage          = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, &Buffer))) return false;

	Buffer->SetPrivateData(WKPDID_D3DDebugObjectName,
		static_cast<UINT>(strlen("ParticleDynamicVB")), "ParticleDynamicVB");
	return true;
}

void FParticleDynamicVertexBuffer::Release()
{
	if (Buffer) { Buffer->Release(); Buffer = nullptr; }
	CapacityBytes  = 0;
	WriteHeadBytes = 0;
	Stride         = 0;
	bMapped        = false;
	MappedPtr      = nullptr;
}

void FParticleDynamicVertexBuffer::BeginFrame(ID3D11DeviceContext* /*Context*/)
{
	// 새 프레임: write head 리셋. 실제 Map은 첫 Allocate에서 DISCARD로 지연.
	WriteHeadBytes = 0;
	bMapped        = false;
	MappedPtr      = nullptr;
}

bool FParticleDynamicVertexBuffer::Allocate(ID3D11DeviceContext* Context, uint32 BytesNeeded,
                                            void*& OutMappedPtr, uint32& OutByteOffset)
{
	OutMappedPtr  = nullptr;
	OutByteOffset = 0;
	if (!Buffer || !Context || BytesNeeded == 0) return false;
	if (WriteHeadBytes + BytesNeeded > CapacityBytes) return false;

	if (!bMapped)
	{
		// 프레임 첫 Allocate — DISCARD로 새 메모리 받기 (이전 프레임 GPU 사용분과 격리)
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(Context->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped))) return false;
		MappedPtr = Mapped.pData;
		bMapped   = true;
	}

	OutMappedPtr  = static_cast<uint8*>(MappedPtr) + WriteHeadBytes;
	OutByteOffset = WriteHeadBytes;
	WriteHeadBytes += BytesNeeded;
	return true;
}

void FParticleDynamicVertexBuffer::EndFrame(ID3D11DeviceContext* Context)
{
	if (bMapped && Buffer && Context)
	{
		Context->Unmap(Buffer, 0);
	}
	bMapped   = false;
	MappedPtr = nullptr;
}
