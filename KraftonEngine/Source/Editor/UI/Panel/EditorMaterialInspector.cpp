#include "EditorMaterialInspector.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "Engine/Materials/Material.h"
#include "Engine/Materials/MaterialDomain.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Texture/Texture2D.h"

FEditorMaterialInspector::FEditorMaterialInspector(std::filesystem::path InPath)
{
	MaterialPath = InPath;
	CachedMaterial = FMaterialManager::Get().GetOrCreateMaterial(
		FPaths::ToUtf8(InPath.lexically_relative(FPaths::RootDir()).generic_wstring())
	);
}

void FEditorMaterialInspector::Render()
{
	bool bIsValid = ImGui::Begin("MaterialInspector");
	bIsValid &= std::filesystem::exists(MaterialPath);
	bIsValid &= (MaterialPath.extension() == ".uasset" || MaterialPath.extension() == ".mat");

	if (!bIsValid)
	{
		ImGui::End();
		return;
	}

	// 바이너리(.uasset) 머티리얼은 JSON 로드가 불가하므로 객체에서 직접 경로를 표시.
	// (파라미터/텍스처 편집은 아래에서 CachedMaterial 객체 기반으로 동작.)
	const FString PathLabel = CachedMaterial ? CachedMaterial->GetAssetPathFileName() : FString();
	ImGui::Selectable(PathLabel.c_str());

	RenderRenderStateSection();
	RenderShaderParameter();
	RenderTextureSection();

	if (CachedMaterial)
	{
		ImGui::Separator();
		if (ImGui::Button("Save"))
		{
			const FString SavePath = CachedMaterial->GetAssetPathFileName();
			if (!SavePath.empty())
			{
				FMaterialManager::Get().SaveMaterial(CachedMaterial, SavePath);
			}
		}
	}

	ImGui::End();
}

namespace
{
	struct FDomainOption { const char* Label; EMaterialDomain Value; };
	struct FBlendOption  { const char* Label; EBlendMode Value; };

	// enum 정렬 순서대로 — 인덱스 = enum 값.
	const FDomainOption GDomainOptions[] = {
		{ "Surface",     EMaterialDomain::Surface },
		{ "PostProcess", EMaterialDomain::PostProcess },
		{ "UI",          EMaterialDomain::UI },
		{ "Decal",       EMaterialDomain::Decal },
	};
	const FBlendOption GBlendOptions[] = {
		{ "Opaque",      EBlendMode::Opaque },
		{ "Masked",      EBlendMode::Masked },
		{ "Translucent", EBlendMode::Translucent },
		{ "Additive",    EBlendMode::Additive },
		{ "Modulate",    EBlendMode::Modulate },
	};
}

void FEditorMaterialInspector::RenderRenderStateSection()
{
	if (!CachedMaterial)
		return;

	// Domain 드롭다운
	const EMaterialDomain CurDomain = CachedMaterial->GetDomain();
	const char* DomainLabel = "";
	for (const FDomainOption& Opt : GDomainOptions)
		if (Opt.Value == CurDomain) { DomainLabel = Opt.Label; break; }

	if (ImGui::BeginCombo("Domain", DomainLabel))
	{
		for (const FDomainOption& Opt : GDomainOptions)
		{
			const bool bSelected = (Opt.Value == CurDomain);
			if (ImGui::Selectable(Opt.Label, bSelected))
				CachedMaterial->SetDomainBlend(Opt.Value, CachedMaterial->GetBlendMode());
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// BlendMode 드롭다운
	const EBlendMode CurBlend = CachedMaterial->GetBlendMode();
	const char* BlendLabel = "";
	for (const FBlendOption& Opt : GBlendOptions)
		if (Opt.Value == CurBlend) { BlendLabel = Opt.Label; break; }

	if (ImGui::BeginCombo("Blend Mode", BlendLabel))
	{
		for (const FBlendOption& Opt : GBlendOptions)
		{
			const bool bSelected = (Opt.Value == CurBlend);
			if (ImGui::Selectable(Opt.Label, bSelected))
				CachedMaterial->SetDomainBlend(CachedMaterial->GetDomain(), Opt.Value);
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// Custom shader 토글
	bool bUseCustom = CachedMaterial->WasCustomShaderRequested();
	if (ImGui::Checkbox("Use Custom Shader", &bUseCustom))
		CachedMaterial->SetUseCustomShader(bUseCustom);

	const FString& ShaderPath = CachedMaterial->GetShaderPathForSerialize();
	ImGui::TextDisabled("Shader: %s", ShaderPath.empty() ? "(none)" : ShaderPath.c_str());

	ImGui::Separator();
}

void FEditorMaterialInspector::RenderShaderParameter()
{
	const auto& Layout = CachedMaterial->GetParameterInfo();

	for (const auto& [ParamName, Info] : Layout)
	{
		ImGui::Text(ParamName.c_str());
		
		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Param;
				bool bIsValid = CachedMaterial->GetScalarParameter(ParamName, Param);
				ImGui::DragFloat("##floatParam", &Param);
				CachedMaterial->SetScalarParameter(ParamName, Param);
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Param;
				bool bIsValid = CachedMaterial->GetVector3Parameter(ParamName, Param);
				ImGui::DragFloat3("##float3Param", &Param.X);
				CachedMaterial->SetVector3Parameter(ParamName, Param);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Param;
				bool bIsValid = CachedMaterial->GetVector4Parameter(ParamName, Param);
				ImGui::DragFloat4("##float4Param", &Param.X);
				CachedMaterial->SetVector4Parameter(ParamName, Param);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
			{
				FMatrix Param;
				bool bIsValid = CachedMaterial->GetMatrixParameter(ParamName, Param);
				ImGui::DragFloat4("##matrix1Param", Param.Data);
				ImGui::DragFloat4("##matrix2Param", Param.Data + 4);
				ImGui::DragFloat4("##matrix3Param", Param.Data + 4);
				ImGui::DragFloat4("##matrix4Param", Param.Data + 4);
				CachedMaterial->SetMatrixParameter(ParamName, Param);
				break;
			}
			default:
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

}

void FEditorMaterialInspector::RenderTextureSection()
{
	TMap<FString, UTexture2D*>* Textures = CachedMaterial->GetTexture();

	for (auto& Pair : *Textures)
	{
		FString SlotName = Pair.first.c_str();
		UTexture2D* Texture = Pair.second;

		if (!Texture)
			continue;


		ImGui::Text(SlotName.c_str());
		ImGui::Image(Texture->GetSRV(), ImVec2(100, 100));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PNGElement"))
			{
				FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(payload->Data);
				FString NewTexturePath = FPaths::ToUtf8(
					ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()
				);
				ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
				const bool bIsColorTexture =
					SlotName == "DiffuseTexture" ||
					SlotName == "EmissiveTexture" ||
					SlotName == "Custom0Texture" ||
					SlotName == "Custom1Texture";
				UTexture2D* NewTexture = UTexture2D::LoadFromFile(
					NewTexturePath,
					Device,
					bIsColorTexture ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
				if (NewTexture)
				{
					CachedMaterial->SetTextureParameter(SlotName, NewTexture);
					CachedMaterial->RebuildCachedSRVs();
				}

			}
			ImGui::EndDragDropTarget();

		}
	}
}
