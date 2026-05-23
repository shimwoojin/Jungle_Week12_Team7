#include "Materials/MaterialInstance.h"

#include "Engine/Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Shader/Shader.h"
#include "Texture/Texture2D.h"

void UMaterialInstance::InitializeFromParent(UMaterial* InParent, const FString& InPathFileName)
{
	if (!InParent) return;
	Parent = InParent;

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	ID3D11DeviceContext* Ctx = GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

	// Parent의 CBMap을 자체 슬롯으로 복제 — 이후 SetXxx은 자체 CB에만 반영.
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Cloned;
	for (const auto& Pair : Parent->ConstantBufferMap)
	{
		auto NewCB = std::make_unique<FMaterialConstantBuffer>();
		NewCB->Init(Device, Pair.second->Size, Pair.second->SlotIndex);
		if (Pair.second->CPUData && NewCB->CPUData)
		{
			memcpy(NewCB->CPUData, Pair.second->CPUData, Pair.second->Size);
		}
		NewCB->bDirty = true;
		NewCB->Upload(Ctx);
		Cloned[Pair.first] = std::move(NewCB);
	}

	Create(InPathFileName,
		Parent->Template,
		Parent->RenderPass,
		Parent->BlendState,
		Parent->DepthStencilState,
		Parent->RasterizerState,
		std::move(Cloned));

	// Texture는 핸들 공유 — UTexture2D 자체가 GPU 리소스라 deep clone 불필요.
	for (const auto& Pair : Parent->TextureParameters)
	{
		TextureParameters[Pair.first] = Pair.second;
	}

	RebuildCachedSRVs();
}
