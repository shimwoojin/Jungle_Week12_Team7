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

		if (Path.extension() != L".uasset") continue;
		if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

		FMaterialAssetListItem Item;
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());

		// .uasset 은 메시/파티클도 공유하는 확장자 → 헤더 Type 으로 Material 만 거른다.
		EAssetPackageType PkgType = EAssetPackageType::Unknown;
		if (!FAssetPackage::GetPackageType(Item.FullPath, PkgType) || PkgType != EAssetPackageType::Material)
			continue;

		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		AvailableMaterialFiles.push_back(std::move(Item));
	}
}

void FMaterialManager::ScanShaderPaths()
{
	AvailableShaderPaths.clear();

	const std::filesystem::path ShaderRoot = FPaths::RootDir() + L"Shaders/";
	if (!std::filesystem::exists(ShaderRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(ShaderRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		if (Path.extension() != L".hlsl") continue; // .hlsli(include) 제외 — 독립 셰이더만

		AvailableShaderPaths.push_back(
			FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring()));
	}
}

void FMaterialManager::ScanTexturePaths()
{
	AvailableTexturePaths.clear();

	const std::filesystem::path TextureRoot = FPaths::RootDir() + L"Content/Texture/";
	if (!std::filesystem::exists(TextureRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(TextureRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		if (Path.extension() != L".png") continue; // 에디터 텍스처 규칙(.png)

		AvailableTexturePaths.push_back(
			FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring()));
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

	// 3. 바이너리가 없으면 기본(핑크) 머티리얼. JSON 변환 경로는 제거됨
	//    (에셋은 .uasset 로 마이그레이션 완료, .mat JSON 미지원).
	UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
	FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
	DefaultMaterial->Create(UassetPath, Template, EMaterialDomain::Surface, EBlendMode::Opaque, std::move(Buffers));
	DefaultMaterial->SetShaderPathForSerialize(DefaultShaderPath);
	DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));
	MaterialCache.emplace(UassetPath, DefaultMaterial);
	return DefaultMaterial;
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

// 임포터용 — JSON 없이 머티리얼을 직접 만들고 .uasset 으로 저장한다.
UMaterial* FMaterialManager::CreateImportedMaterialAsset(const FString& UassetPath, const FVector4& SectionColor,
	const FString& DiffuseTexturePath, const FString& NormalTexturePath)
{
	FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
	if (!Template) return nullptr;
	auto Buffers = CreateConstantBuffers(Template);

	UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Create(UassetPath, Template, EMaterialDomain::Surface, EBlendMode::Opaque, std::move(Buffers));
	Material->SetShaderPathForSerialize(DefaultShaderPath);
	Material->SetVector4Parameter("SectionColor", SectionColor);
	Material->SetScalarParameter("HasNormalMap", NormalTexturePath.empty() ? 0.0f : 1.0f);

	if (!DiffuseTexturePath.empty())
		if (UTexture2D* Tex = UTexture2D::LoadFromFile(DiffuseTexturePath, Device, ETextureColorSpace::SRGB))
			Material->SetTextureParameter("DiffuseTexture", Tex);
	if (!NormalTexturePath.empty())
		if (UTexture2D* Tex = UTexture2D::LoadFromFile(NormalTexturePath, Device, ETextureColorSpace::Linear))
			Material->SetTextureParameter("NormalTexture", Tex);

	Material->RebuildCachedSRVs();
	SaveMaterial(Material, UassetPath);
	MaterialCache.emplace(UassetPath, Material);
	return Material;
}

// 에디터 Create Material 팩토리용 — 흰색 기본 머티리얼(텍스처 없음)을 생성·저장·캐시.
UMaterial* FMaterialManager::CreateMaterialAsset(const FString& UassetPath)
{
	return CreateImportedMaterialAsset(UassetPath, FVector4(1.0f, 1.0f, 1.0f, 1.0f), FString(), FString());
}

// 머티리얼의 셰이더(레이아웃 소스 & custom 대상)를 교체한다. 템플릿/CB 를 재구성하므로
// 레이아웃이 달라지면 파라미터 값은 초기화되고 텍스처 슬롯은 유지(RebuildCachedSRVs).
// 컴파일 실패(부적합 셰이더)거나 인스턴스면 변경을 거부한다.
bool FMaterialManager::SetMaterialShader(UMaterial* Material, const FString& ShaderPath)
{
	if (!Material || Material->IsMaterialInstance())
		return false;

	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
	if (!Template)
		return false; // FindOrCreate 실패(엔트리포인트 없음/컴파일 오류 등) → 변경 거부

	auto Buffers = CreateConstantBuffers(Template);
	Material->Create(Material->GetAssetPathFileName(), Template,
		Material->GetDomain(), Material->GetBlendMode(), std::move(Buffers));
	Material->SetShaderPathForSerialize(ShaderPath);

	if (Material->WasCustomShaderRequested())
		Material->SetCustomShader(Template->GetShader());

	Material->RebuildCachedSRVs();
	return true;
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
