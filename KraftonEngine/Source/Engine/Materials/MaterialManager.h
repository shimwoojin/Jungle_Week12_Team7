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

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }

	void Release();
private:
	// 셰이더로 Template 생성 또는 캐시에서 반환
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath);

	// 바이너리(.uasset) 직렬화 — exemplar = ParticleSystemManager.
	bool       SaveMaterial(UMaterial* Material, const FString& UassetPath);
	UMaterial* LoadMaterialBinary(const FString& UassetPath);

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

	const FString DefaultShaderPath = "Shaders/Geometry/UberLit.hlsl";


};