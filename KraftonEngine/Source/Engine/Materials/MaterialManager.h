#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include <memory>

#include "Render/Types/RenderStateTypes.h"

class FMaterialTemplate;
class UMaterial;
struct FMaterialConstantBuffer;
struct FVector4;

struct FMaterialAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

class FMaterialManager : public TSingleton<FMaterialManager>
{
	friend class TSingleton<FMaterialManager>;

    TMap<FString, FMaterialTemplate*> TemplateCache;    // 셰이더 경로 → Template (공유)
	TMap<FString, UMaterial*> MaterialCache;	//MatFilePath
	TArray<FMaterialAssetListItem> AvailableMaterialFiles;
	TArray<FString> AvailableShaderPaths;
	TArray<FString> AvailableTexturePaths;

	ID3D11Device* Device = nullptr;

public:
	~FMaterialManager(); // 선언만 남김

	void Initialize(ID3D11Device* InDevice) { Device = InDevice; }

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device);

    // UMaterial 생성
	UMaterial* GetOrCreateMaterial(const FString& MatFilePath);

	// 임포터용 — JSON 없이 머티리얼을 직접 만들고 .uasset 으로 저장.
	UMaterial* CreateImportedMaterialAsset(const FString& UassetPath, const FVector4& SectionColor,
		const FString& DiffuseTexturePath, const FString& NormalTexturePath);

	// 에디터 Create Material 팩토리용 — 빈(흰색) 머티리얼을 만들어 .uasset 으로 저장.
	UMaterial* CreateMaterialAsset(const FString& UassetPath);

	// 바이너리(.uasset) 저장 — 인스펙터 Save 버튼 / 팩토리에서 호출.
	bool SaveMaterial(UMaterial* Material, const FString& UassetPath);

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }

	// 머티리얼 에디터 셰이더 선택용 — Shaders/ 하위 .hlsl(독립 셰이더, .hlsli include 제외) 열거.
	void ScanShaderPaths();
	const TArray<FString>& GetAvailableShaderPaths() const { return AvailableShaderPaths; }

	// 머티리얼 에디터 텍스처 선택용 — Content/ 하위 이미지(.png) 열거.
	void ScanTexturePaths();
	const TArray<FString>& GetAvailableTexturePaths() const { return AvailableTexturePaths; }

	// 머티리얼의 셰이더(=레이아웃 소스 & custom 대상) 교체 — 템플릿/CB 재구성. 실패 시 false.
	bool SetMaterialShader(UMaterial* Material, const FString& ShaderPath);

	void Release();
private:
	// 셰이더로 Template 생성 또는 캐시에서 반환
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath);

	// 바이너리(.uasset) 직렬화 — exemplar = ParticleSystemManager. (SaveMaterial 은 public: 에디터 Save)
	UMaterial* LoadMaterialBinary(const FString& UassetPath);

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

	const FString DefaultShaderPath = "Shaders/Geometry/UberLit.hlsl";


};