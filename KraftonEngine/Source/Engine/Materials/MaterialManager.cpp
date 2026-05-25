#include "MaterialManager.h"
#include <filesystem>
#include <fstream>
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialDomain.h"  // Phase 1: Domain/BlendMode 도출 (dormant)
#include "Platform/Paths.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Texture/Texture2D.h"
#include "Render/Pipeline/Renderer.h"
#include "Serialization/WindowsArchive.h"  // Phase 4: 바이너리 .uasset
#include "Asset/AssetPackage.h"

namespace
{
	// ".mat" → ".uasset" 정규화(이미 .uasset 이면 그대로). 캐시 키 + 바이너리 타겟.
	// 메시 임베드/하드코딩 legacy ".mat" 참조가 자동으로 ".uasset" 을 가리키게 한다.
	FString NormalizeMatToUasset(const FString& Path)
	{
		std::filesystem::path P(FPaths::ToWide(Path));
		if (P.extension() == L".mat") P.replace_extension(L".uasset");
		return FPaths::ToUtf8(P.generic_wstring());
	}
	// ".uasset" → ".mat" (legacy JSON 경로). 이미 .mat 이면 그대로.
	FString ToLegacyMatPath(const FString& Path)
	{
		std::filesystem::path P(FPaths::ToWide(Path));
		if (P.extension() == L".uasset") P.replace_extension(L".mat");
		return FPaths::ToUtf8(P.generic_wstring());
	}
}

void FMaterialManager::ScanMaterialAssets()
{
	AvailableMaterialFiles.clear();

	const std::filesystem::path MaterialRoot = FPaths::RootDir() + L"Content/Material/";

	if (!std::filesystem::exists(MaterialRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MaterialRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();

		if (Path.extension() != L".mat") continue;
		if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

		FMaterialAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMaterialFiles.push_back(std::move(Item));
	}
}

UMaterial* FMaterialManager::GetOrCreateMaterial(const FString& MatFilePath)
{
	// 0. 경로 정규화: .mat → .uasset (캐시 키 = .uasset). legacy .mat 참조 호환.
	const FString UassetPath = NormalizeMatToUasset(MatFilePath);

	// 1. 캐시 반환
	auto It = MaterialCache.find(UassetPath);
	if (It != MaterialCache.end())
	{
		return It->second;
	}

	// 2. 바이너리 .uasset 존재 시 우선 로드
	if (std::filesystem::exists(FPaths::ToWide(FPaths::MakeProjectRelative(UassetPath))))
	{
		if (UMaterial* Bin = LoadMaterialBinary(UassetPath))
		{
			MaterialCache.emplace(UassetPath, Bin);
			return Bin;
		}
	}

	// 3. legacy JSON(.mat) fallback — 첫 접근 시 바이너리로 변환(lazy).
	const FString MatPath = ToLegacyMatPath(MatFilePath);
	json::JSON JsonData = ReadJsonFile(MatPath);
	if (JsonData.IsNull())
	{
		// 기본 머티리얼 (디스크 저장 안 함)
		UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
		FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
		DefaultMaterial->Create(UassetPath, Template, EMaterialDomain::Surface, EBlendMode::Opaque, std::move(Buffers));
		DefaultMaterial->SetShaderPathForSerialize(DefaultShaderPath);
		DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));
		MaterialCache.emplace(UassetPath, DefaultMaterial);
		return DefaultMaterial;
	}

	// 3.5 Parent 키 — UMaterialInstance 분기
	if (JsonData.hasKey(MatKeys::Parent))
	{
		FString ParentPath = JsonData[MatKeys::Parent].ToString().c_str();
		if (!ParentPath.empty())
		{
			UMaterial* ParentMat = GetOrCreateMaterial(ParentPath);
			if (ParentMat)
			{
				UMaterialInstance* MI = UObjectManager::Get().CreateObject<UMaterialInstance>();
				MI->InitializeFromParent(ParentMat, UassetPath);

				if (JsonData.hasKey(MatKeys::RenderPass))
				{
					ERenderPass MIPass = StringToRenderPass(JsonData[MatKeys::RenderPass].ToString().c_str());
					MI->OverrideRenderPass(MIPass);
					if (JsonData.hasKey(MatKeys::BlendState))
						MI->OverrideBlendState(StringToBlendState(JsonData[MatKeys::BlendState].ToString().c_str(), MIPass));
					if (JsonData.hasKey(MatKeys::DepthStencilState))
						MI->OverrideDepthStencilState(StringToDepthStencilState(JsonData[MatKeys::DepthStencilState].ToString().c_str(), MIPass));
					if (JsonData.hasKey(MatKeys::RasterizerState))
						MI->OverrideRasterizerState(StringToRasterizerState(JsonData[MatKeys::RasterizerState].ToString().c_str(), MIPass));
				}

				ApplyParameters(MI, JsonData);
				ApplyTextures(MI, JsonData);
				MI->RebuildCachedSRVs();

				MaterialCache.emplace(UassetPath, MI);
				SaveMaterial(MI, UassetPath);  // lazy 변환
				return MI;
			}
		}
	}

	// 4. 기본 정보 추출 + Domain/BlendMode 역매핑
	FString ShaderPath = JsonData[MatKeys::ShaderPath].ToString().c_str();
	FString RenderPassStr = JsonData[MatKeys::RenderPass].ToString().c_str();
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);
	FString BlendStr = JsonData.hasKey(MatKeys::BlendState) ? JsonData[MatKeys::BlendState].ToString().c_str() : "";
	FString RasterStr = JsonData.hasKey(MatKeys::RasterizerState) ? JsonData[MatKeys::RasterizerState].ToString().c_str() : "";
	EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
	ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);
	EMaterialDomain Domain; EBlendMode BlendMode;
	DeriveDomainBlend(RenderPass, BlendState, Domain, BlendMode);

	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
	if (!Template) return nullptr;
	auto InjectedBuffers = CreateConstantBuffers(Template);

	UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Create(UassetPath, Template, Domain, BlendMode, std::move(InjectedBuffers));
	Material->SetShaderPathForSerialize(ShaderPath);
	if (RasterState != Material->GetRasterizerState())
		Material->SetRasterOverride(RasterState);
	{
		const bool bUber     = (ShaderPath == DefaultShaderPath);
		const bool bParticle = (ShaderPath == FString(EShaderPath::ParticleSprite)
			|| ShaderPath == FString(EShaderPath::ParticleMesh)
			|| ShaderPath == FString(EShaderPath::ParticleBeamTrail));
		if (!bUber && !bParticle && Template)
			Material->SetCustomShader(Template->GetShader());
	}
	MaterialCache.emplace(UassetPath, Material);

	InjectDefaultParameters(JsonData, Template, Material);
	PurgeStaleParameters(JsonData, Template);
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);
	Material->RebuildCachedSRVs();

	SaveMaterial(Material, UassetPath);  // lazy 변환 (JSON → 바이너리)
	return Material;
}

// ============================================================
// 바이너리(.uasset) 직렬화 — exemplar = FParticleSystemManager
// ============================================================
bool FMaterialManager::SaveMaterial(UMaterial* Material, const FString& UassetPath)
{
	if (!Material) return false;

	const FString NormalizedPath = FPaths::MakeProjectRelative(UassetPath);
	FWindowsBinWriter Ar(NormalizedPath);
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::Material);
	FAssetImportMetadata Metadata;
	Ar << Header;
	Ar << Metadata;

	UMaterialInstance* MI = Cast<UMaterialInstance>(Material);
	bool bIsInstance = (MI != nullptr);
	Ar << bIsInstance;

	FString PathFileName = Material->GetAssetPathFileName();
	Ar << PathFileName;

	if (bIsInstance)
	{
		FString ParentPath = MI->GetParent() ? NormalizeMatToUasset(MI->GetParent()->GetAssetPathFileName()) : FString();
		Ar << ParentPath;
	}
	else
	{
		FString ShaderPath = Material->GetShaderPathForSerialize();
		Ar << ShaderPath;
	}

	Material->Serialize(Ar);  // Domain/BlendMode/custom-flag/override + params(CPUData) + textures
	return Ar.IsValid();
}

UMaterial* FMaterialManager::LoadMaterialBinary(const FString& UassetPath)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(UassetPath);
	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid()) return nullptr;

	FAssetPackageHeader Header;
	Ar << Header;
	if (!Header.IsValid(EAssetPackageType::Material)) return nullptr;
	FAssetImportMetadata Metadata;
	Ar << Metadata;

	bool bIsInstance = false;
	Ar << bIsInstance;
	FString PathFileName;
	Ar << PathFileName;

	if (bIsInstance)
	{
		FString ParentPath;
		Ar << ParentPath;
		UMaterial* ParentMat = GetOrCreateMaterial(ParentPath);
		if (!ParentMat) return nullptr;

		UMaterialInstance* MI = UObjectManager::Get().CreateObject<UMaterialInstance>();
		MI->InitializeFromParent(ParentMat, PathFileName);  // Template/CB 를 Parent 에서 복제
		MI->Serialize(Ar);                                   // 복제된 CB 에 override/CPUData/텍스처 기록
		if (!Ar.IsValid()) { UObjectManager::Get().DestroyObject(MI); return nullptr; }
		return MI;
	}

	FString ShaderPath;
	Ar << ShaderPath;
	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);  // 순서 의존성: 먼저 Template
	if (!Template) return nullptr;
	auto Buffers = CreateConstantBuffers(Template);                // 빈 CB 선생성

	UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Create(PathFileName, Template, EMaterialDomain::Surface, EBlendMode::Opaque, std::move(Buffers));
	Material->SetShaderPathForSerialize(ShaderPath);
	Material->Serialize(Ar);  // Domain/BlendMode 등을 덮어쓰고 CPUData 를 빈 CB 에 기록
	if (!Ar.IsValid()) { UObjectManager::Get().DestroyObject(Material); return nullptr; }

	if (Material->WasCustomShaderRequested())
		Material->SetCustomShader(Template->GetShader());

	return Material;
}

json::JSON FMaterialManager::ReadJsonFile(const FString& FilePath) const
{
	std::ifstream File(FPaths::ToWide(FilePath).c_str());
	if (!File.is_open()) return json::JSON(); // Null JSON 반환

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	return json::JSON::Load(Buffer.str());
}

TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> FMaterialManager::CreateConstantBuffers(FMaterialTemplate* Template)
{

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> InjectedBuffers;

	const auto& RequiredBuffers = Template->GetParameterInfo();
	std::vector<FString> CreatedBuffers;

	for (const auto& BufferInfo : RequiredBuffers)
	{
		const FMaterialParameterInfo* ParamInfo = BufferInfo.second;

		if (std::find(CreatedBuffers.begin(), CreatedBuffers.end(), ParamInfo->BufferName) != CreatedBuffers.end())
			continue;

		auto MatCB = std::make_unique<FMaterialConstantBuffer>();
		MatCB->Init(Device, ParamInfo->BufferSize, ParamInfo->SlotIndex);

		InjectedBuffers.emplace(ParamInfo->BufferName, std::move(MatCB));
		CreatedBuffers.push_back(ParamInfo->BufferName);
	}

	return InjectedBuffers;
}

void FMaterialManager::ApplyParameters(UMaterial* Material, json::JSON& JsonData)
{
	if (!JsonData.hasKey(MatKeys::Parameters)) return;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		json::JSON& Value = Pair.second;

		if (Value.JSONType() == json::JSON::Class::Array)
		{
			if (Value.length() == 3)
			{
				Material->SetVector3Parameter(ParamName, FVector((float)Value[0].ToFloat(), (float)Value[1].ToFloat(), (float)Value[2].ToFloat()));
			}
			else if (Value.length() == 4)
			{
				Material->SetVector4Parameter(ParamName, FVector4((float)Value[0].ToFloat(), (float)Value[1].ToFloat(), (float)Value[2].ToFloat(), (float)Value[3].ToFloat()));
			}
		}
		else if (Value.JSONType() == json::JSON::Class::Floating || Value.JSONType() == json::JSON::Class::Integral)
		{
			Material->SetScalarParameter(ParamName, (float)Value.ToFloat());
		}
	}
}

void FMaterialManager::ApplyTextures(UMaterial* Material, json::JSON& JsonData)
{
	if (!JsonData.hasKey(MatKeys::Textures)) return;

	for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
		FString TexturePath = Pair.second.ToString().c_str();
		const bool bIsColorTexture =
			SlotName == "DiffuseTexture" ||
			SlotName == "EmissiveTexture" ||
			SlotName == "Custom0Texture" ||
			SlotName == "Custom1Texture";

		UTexture2D* Texture = UTexture2D::LoadFromFile(
			TexturePath,
			Device,
			bIsColorTexture ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
		if (Texture)
		{
			Material->SetTextureParameter(SlotName, Texture);
		}
	}
}


ERenderPass FMaterialManager::StringToRenderPass(const FString& Str) const
{
	using namespace RenderStateStrings;
	return FromString(RenderPassMap, Str, ERenderPass::Opaque);
}

EBlendState FMaterialManager::StringToBlendState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(BlendStateMap, Str, EBlendState::Opaque);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::Translucent:
	case ERenderPass::Decal:
	case ERenderPass::EditorLines:
	case ERenderPass::PostProcess:
	case ERenderPass::GizmoInner:
	case ERenderPass::OverlayFont:
		return EBlendState::AlphaBlend;
	case ERenderPass::AdditiveDecal:
		return EBlendState::Additive;
	case ERenderPass::SelectionMask:
		return EBlendState::NoColor;
	default:
		return EBlendState::Opaque;
	}
}

EDepthStencilState FMaterialManager::StringToDepthStencilState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(DepthStencilStateMap, Str, EDepthStencilState::Default);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
		return EDepthStencilState::DepthReadOnly;
	case ERenderPass::SelectionMask:
		return EDepthStencilState::StencilWrite;
	case ERenderPass::PostProcess:
	case ERenderPass::OverlayFont:
		return EDepthStencilState::NoDepth;
	case ERenderPass::GizmoOuter:
		return EDepthStencilState::GizmoOutside;
	case ERenderPass::GizmoInner:
		return EDepthStencilState::GizmoInside;
	default:
		return EDepthStencilState::Default;
	}
}

ERasterizerState FMaterialManager::StringToRasterizerState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(RasterizerStateMap, Str, ERasterizerState::SolidBackCull);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
	case ERenderPass::SelectionMask:
	case ERenderPass::PostProcess:
		return ERasterizerState::SolidNoCull;
	default:
		return ERasterizerState::SolidBackCull;
	}
}

void FMaterialManager::SaveToJSON(json::JSON& JsonData, const FString& MatFilePath)
{
	std::ofstream File(FPaths::ToWide(MatFilePath));
	File << JsonData.dump();
}

bool FMaterialManager::InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material)
{
	const auto& Layout = Template->GetParameterInfo();
	bool bInjected = false;

	for (const auto& Pair : Layout)
	{
		const FString& ParamName = Pair.first;
		const FMaterialParameterInfo* Info = Pair.second;

		// 이미 JSON에 있으면 스킵
		if (!JsonData[MatKeys::Parameters][ParamName].IsNull())
			continue;

		bInjected = true;

		if (ParamName == "SectionColor")
		{
			JsonData[MatKeys::Parameters][ParamName] = json::Array(1.0f, 1.0f, 1.0f, 1.0f);
			continue;
		}

		if (ParamName == "HasNormalMap")
		{
			JsonData[MatKeys::Parameters][ParamName] = 0.0f;
			continue;
		}

		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Value = 0.f;
				Material->GetScalarParameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = Value;
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Value;
				Material->GetVector3Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Value;
				Material->GetVector4Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
			{
				FMatrix Value;
				Material->GetMatrixParameter(ParamName, Value);
				auto MatArray = json::Array();
				for (int i = 0; i < 16; ++i)
					MatArray.append(Value.Data[i]);
				JsonData[MatKeys::Parameters][ParamName] = MatArray;
				break;
			}
			default:
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

	return bInjected;
}

bool FMaterialManager::PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template)
{
	if (!JsonData.hasKey(MatKeys::Parameters)) return false;

	const auto& Layout = Template->GetParameterInfo();
	json::JSON CleanParams = json::JSON::Make(json::JSON::Class::Object);
	bool bPurged = false;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		if (Layout.find(ParamName) != Layout.end())
		{
			CleanParams[Pair.first] = Pair.second;
		}
		else
		{
			bPurged = true;
		}
	}

	if (bPurged)
	{
		JsonData[MatKeys::Parameters] = std::move(CleanParams);
	}

	return bPurged;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath)
{
	// 1. 템플릿이 캐시에 있는지 확인 (셰이더 경로를 키값으로 사용)
	auto It = TemplateCache.find(ShaderPath);
	if (It != TemplateCache.end())
	{
		return It->second;
	}

	// 2. 템플릿이 기존에 없다면 새로 제작
	//    캐시에 있으면 반환, 없으면 컴파일 후 캐싱
	FShader* Shader = FShaderManager::Get().FindOrCreate(ShaderPath);
	if (!Shader)
	{
		return nullptr;
	}

	FMaterialTemplate* NewTemplate = new FMaterialTemplate();
	NewTemplate->Create(Shader);
	TemplateCache.emplace(ShaderPath, NewTemplate);
	return NewTemplate;
}

FMaterialManager::~FMaterialManager()
{
	if (!Device)
	{
		Release();
	}

}

void FMaterialManager::Release()
{
	// 1. TemplateCache 메모리 해제
	// GetOrCreateTemplate()에서 new FMaterialTemplate()로 직접 할당했으므로 여기서 delete 해줍니다.
	for (auto& Pair : TemplateCache)
	{
		if (Pair.second != nullptr)
		{
			delete Pair.second;
			Pair.second = nullptr;
		}
	}

	TemplateCache.clear();

	// 2. GPU 버퍼를 Device 해제 전에 명시 해제, UObject 수명은 UObjectManager가 관리
	for (auto& [Key, Mat] : MaterialCache)
	{
		if (Mat) Mat->ReleaseGPUBuffers();
	}
	MaterialCache.clear();

	// 3. Device 참조 해제
	// 외부에서 주입받은 리소스이므로 포인터만 초기화합니다.
	Device = nullptr;
}
