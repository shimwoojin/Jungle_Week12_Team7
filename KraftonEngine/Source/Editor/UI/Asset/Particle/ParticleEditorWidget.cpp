#include "ParticleEditorWidget.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Core/TickFunction.h"
#include "Render/Scene/FScene.h"

#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemManager.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/Modules/ParticleModuleAcceleration.h"

#include "Particle/Modules/ParticleModuleCollision.h"
#include "Particle/Modules/ParticleModuleColor.h"
#include "Particle/Modules/ParticleModuleEventGenerator.h"
#include "Particle/Modules/ParticleModuleLifetime.h"
#include "Particle/Modules/ParticleModuleLocation.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSize.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/Modules/ParticleModuleSubUV.h"
#include "Particle/Modules/ParticleModuleVelocity.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam.h"
#include "Particle/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particle/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Materials/Material.h" // ShaderPathForSerialize (RequiredModule 머티리얼 필터)

namespace
{
	constexpr int32 ModuleTokenRequired = -100;
	constexpr int32 ModuleTokenSpawn    = -101;
	constexpr int32 ModuleTokenTypeData = -102;

	uint32 GNextParticleEditorInstanceId = 0;

	const char* CategoryName(UParticleModule::EModuleCategory Category)
	{
		switch (Category)
		{
		case UParticleModule::EModuleCategory::Required:  return "Required";
		case UParticleModule::EModuleCategory::TypeData:  return "TypeData";
		case UParticleModule::EModuleCategory::Spawn:     return "Spawn";
		case UParticleModule::EModuleCategory::Lifetime:  return "Lifetime";
		case UParticleModule::EModuleCategory::Location:  return "Location";
		case UParticleModule::EModuleCategory::Velocity:  return "Velocity";
		case UParticleModule::EModuleCategory::Acceleration:  return "Const Acceleration";
		case UParticleModule::EModuleCategory::Color:     return "Color";
		case UParticleModule::EModuleCategory::Size:      return "Size";
		case UParticleModule::EModuleCategory::Rotation:  return "Rotation";
		case UParticleModule::EModuleCategory::Collision: return "Collision";
		case UParticleModule::EModuleCategory::Event:     return "Event";
		case UParticleModule::EModuleCategory::SubUV:     return "SubUV";
		default:                                         return "Module";
		}
	}

	ImU32 CategoryColor(UParticleModule::EModuleCategory Category, bool bSelected, bool bEnabled)
	{
		ImU32 Color = IM_COL32(64, 64, 64, 255);
		switch (Category)
		{
		case UParticleModule::EModuleCategory::Required:  Color = IM_COL32(202, 209, 94, 255);  break;
		case UParticleModule::EModuleCategory::Spawn:     Color = IM_COL32(185, 92, 92, 255);   break;
		case UParticleModule::EModuleCategory::TypeData:  Color = IM_COL32(104, 205, 138, 255); break;
		case UParticleModule::EModuleCategory::Lifetime:  Color = IM_COL32(83, 83, 93, 255);    break;
		case UParticleModule::EModuleCategory::Location:  Color = IM_COL32(83, 83, 93, 255);    break;
		case UParticleModule::EModuleCategory::Velocity:  Color = IM_COL32(83, 83, 93, 255);    break;
		case UParticleModule::EModuleCategory::Acceleration:  Color = IM_COL32(83, 83, 93, 255);    break;
		case UParticleModule::EModuleCategory::Color:     Color = IM_COL32(56, 105, 56, 255);   break;
		case UParticleModule::EModuleCategory::Size:      Color = IM_COL32(56, 105, 56, 255);   break;
		case UParticleModule::EModuleCategory::Collision: Color = IM_COL32(70, 70, 86, 255);    break;
		case UParticleModule::EModuleCategory::Event:     Color = IM_COL32(72, 94, 120, 255);   break;
		case UParticleModule::EModuleCategory::SubUV:     Color = IM_COL32(72, 94, 120, 255);   break;
		default: break;
		}

		if (!bEnabled)
		{
			Color = IM_COL32(46, 46, 48, 255);
		}
		if (bSelected)
		{
			Color = IM_COL32(224, 119, 55, 255);
		}
		return Color;
	}

	void CopyToBuffer(char* Buffer, size_t BufferSize, const FString& Text)
	{
		if (!Buffer || BufferSize == 0) return;
		std::snprintf(Buffer, BufferSize, "%s", Text.c_str());
	}

	bool InputTextFString(const char* Label, FString& Value)
	{
		char Buffer[512] = {};
		CopyToBuffer(Buffer, sizeof(Buffer), Value);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			Value = Buffer;
			return true;
		}
		return false;
	}

	bool InputTextSoftObject(const char* Label, FSoftObjectPtr& Value)
	{
		FString Path = Value.ToString();
		if (Path.empty()) Path = "None";
		if (InputTextFString(Label, Path))
		{
			Value.SetPath(Path.empty() ? FString("None") : Path);
			return true;
		}
		return false;
	}

	template<typename TAssetItem>
	bool AssetComboField(const char* Label, FSoftObjectPtr& Value, const TArray<TAssetItem>& Assets)
	{
		FString CurrentPath = Value.ToString();
		if (CurrentPath.empty())
		{
			CurrentPath = "None";
		}

		bool bChanged = false;
		const char* Preview = CurrentPath.c_str();
		if (ImGui::BeginCombo(Label, Preview))
		{
			const bool bSelectedNone = (CurrentPath == "None");
			if (ImGui::Selectable("None", bSelectedNone))
			{
				Value.SetPath("None");
				CurrentPath = "None";
				bChanged = true;
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			for (const TAssetItem& Item : Assets)
			{
				const bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					Value.SetPath(Item.FullPath);
					CurrentPath = Item.FullPath;
					bChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Item.FullPath.c_str());
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	bool MaterialComboField(const char* Label, FSoftObjectPtr& Value)
	{
		return AssetComboField(Label, Value, FMaterialManager::Get().GetAvailableMaterialFiles());
	}

	// emitter 타입(TypeDataModule 클래스)이 강제하는 파티클 셰이더 경로 — ResolveSectionShader 와 동일 매핑.
	FString ParticleForcedShaderPath(const UParticleLODLevel* LOD)
	{
		if (!LOD || LOD->TypeDataModule == nullptr)
			return "Shaders/Particle/Sprite.hlsl";                       // Sprite (TypeData 없음 = 기본)
		if (Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule))
			return "Shaders/Particle/Mesh.hlsl";                         // Mesh
		return "Shaders/Particle/BeamTrail.hlsl";                        // Beam / Ribbon
	}

	// emitter 강제 셰이더와 레이아웃(ShaderPathForSerialize)이 일치하는 머티리얼만 노출하는 콤보.
	// (콤보가 열렸을 때만 머티리얼을 load → 캐시됨. 불일치 머티리얼은 셰이더 레이아웃이 안 맞아 사용 불가.)
	bool MaterialComboFieldFiltered(const char* Label, FSoftObjectPtr& Value, const FString& RequiredShaderPath)
	{
		FString CurrentPath = Value.ToString();
		if (CurrentPath.empty()) CurrentPath = "None";

		bool bChanged = false;
		if (ImGui::BeginCombo(Label, CurrentPath.c_str()))
		{
			const bool bSelectedNone = (CurrentPath == "None");
			if (ImGui::Selectable("None", bSelectedNone)) { Value.SetPath("None"); CurrentPath = "None"; bChanged = true; }
			if (bSelectedNone) ImGui::SetItemDefaultFocus();

			for (const FMaterialAssetListItem& Item : FMaterialManager::Get().GetAvailableMaterialFiles())
			{
				UMaterial* Mat = FMaterialManager::Get().GetOrCreateMaterial(Item.FullPath);
				if (!Mat || Mat->GetShaderPathForSerialize() != RequiredShaderPath)
					continue; // emitter 강제 셰이더와 레이아웃 불일치 → 제외

				const bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected)) { Value.SetPath(Item.FullPath); CurrentPath = Item.FullPath; bChanged = true; }
				if (bSelected) ImGui::SetItemDefaultFocus();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Item.FullPath.c_str());
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	bool MeshComboField(const char* Label, FSoftObjectPtr& Value)
	{
		return AssetComboField(Label, Value, FMeshManager::Get().GetAvailableStaticMeshFiles());
	}

	bool DragFloat3Field(const char* Label, FVector& Value, float Speed = 0.1f, float Min = 0.0f, float Max = 0.0f)
	{
		float Data[3] = { Value.X, Value.Y, Value.Z };
		bool bChanged = false;
		if (Min < Max)
		{
			bChanged = ImGui::DragFloat3(Label, Data, Speed, Min, Max, "%.3f");
		}
		else
		{
			bChanged = ImGui::DragFloat3(Label, Data, Speed, 0.0f, 0.0f, "%.3f");
		}

		if (bChanged)
		{
			Value.X = Data[0];
			Value.Y = Data[1];
			Value.Z = Data[2];
		}
		return bChanged;
	}

	bool Color4Field(const char* Label, FVector4& Value)
	{
		float Data[4] = { Value.R, Value.G, Value.B, Value.A };
		if (ImGui::ColorEdit4(Label, Data))
		{
			Value.R = Data[0];
			Value.G = Data[1];
			Value.B = Data[2];
			Value.A = Data[3];
			return true;
		}
		return false;
	}

	bool ComboInt(const char* Label, int32& Value, const char* const* Names, int32 Count)
	{
		if (Value < 0) Value = 0;
		if (Value >= Count) Value = Count - 1;
		const char* Preview = (Value >= 0 && Value < Count) ? Names[Value] : "Unknown";
		bool bChanged = false;
		if (ImGui::BeginCombo(Label, Preview))
		{
			for (int32 i = 0; i < Count; ++i)
			{
				const bool bSelected = (Value == i);
				if (ImGui::Selectable(Names[i], bSelected))
				{
					Value = i;
					bChanged = true;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	bool HasCategory(UParticleLODLevel* LOD, UParticleModule::EModuleCategory Category)
	{
		if (!LOD) return false;
		if (LOD->RequiredModule && LOD->RequiredModule->GetCategory() == Category) return true;
		if (LOD->SpawnModule && LOD->SpawnModule->GetCategory() == Category) return true;
		if (LOD->TypeDataModule && LOD->TypeDataModule->GetCategory() == Category) return true;
		return false;
	}

	template<typename TModule>
	TModule* CreateParticleModule(UParticleLODLevel* LOD, UParticleEmitter* Owner)
	{
		if (!LOD) return nullptr;
		TModule* Module = UObjectManager::Get().CreateObject<TModule>(LOD);
		if (!Module) return nullptr;
		Module->SetToSensibleDefaults(Owner);
		return Module;
	}

	FString ModuleSelectionLabel(int32 Token)
	{
		switch (Token)
		{
		case ModuleTokenRequired: return "Required";
		case ModuleTokenSpawn:    return "Spawn";
		case ModuleTokenTypeData: return "TypeData";
		default:                 return Token >= 0 ? ("Module[" + std::to_string(Token) + "]") : "Emitter";
		}
	}
}

FParticleEditorWidget::FParticleEditorWidget()
	: InstanceId(static_cast<int32>(GNextParticleEditorInstanceId++))
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("ParticleEditorPreview_" + Id);
	WindowIdSuffix = "###ParticleEditor_" + Id;
}

FParticleEditorWidget::~FParticleEditorWidget()
{
	Close();
}

bool FParticleEditorWidget::CanEdit(UObject* Object) const
{
	return Cast<UParticleSystem>(Object) != nullptr;
}

bool FParticleEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UParticleSystem* Current = Cast<UParticleSystem>(EditedObject);
	const UParticleSystem* Requested = Cast<UParticleSystem>(Object);
	if (!IsOpen() || !Current || !Requested)
	{
		return false;
	}

	const FString& CurrentPath = Current->GetSourcePath();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == Requested->GetSourcePath();
}

void FParticleEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	if (!IsOpen())
	{
		return;
	}

	SelectedEmitterIndex = -1;
	SelectedModuleIndex  = -1;
	CurrentLODIndex      = 0;
	bSimPlaying          = true;
	bPendingClose        = false;

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	PreviewActor = WorldContext.World->SpawnActor<AActor>();
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
	PreviewActor->bTickInEditor = true;

	PreviewParticleComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	PreviewActor->SetRootComponent(PreviewParticleComponent);
	PreviewParticleComponent->SetTemplate(GetEditedSystem());
	PreviewParticleComponent->Activate(true);

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(),
		static_cast<uint32>((std::max)(1.0f, ViewportSize.x)),
		static_cast<uint32>((std::max)(1.0f, ViewportSize.y)));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(PreviewActor);
	ViewportClient.SetPreviewMeshComponent(nullptr);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	FSlateApplication::Get().RegisterViewport(&ViewportClient);

	if (UParticleSystem* System = GetEditedSystem())
	{
		System->BuildEmitters();
	}
}

void FParticleEditorWidget::Close()
{
	if (!IsOpen() && !ViewportClient.IsRenderable())
	{
		return;
	}

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);
		PreviewWorld->SetEditorPOVProvider(nullptr);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
	PreviewActor = nullptr;
	PreviewParticleComponent = nullptr;

	FAssetEditorWidget::Close();
}

void FParticleEditorWidget::Tick(float DeltaTime)
{
	if (!IsOpen())
	{
		return;
	}

	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}

	if (bSimPlaying && PreviewParticleComponent && PreviewParticleComponent->IsActive())
	{
		const float SafeSpeed = (std::max)(0.0f, SimulationSpeed);
		PreviewParticleComponent->TickComponent(DeltaTime * SafeSpeed, LEVELTICK_ViewportsOnly,
			PreviewParticleComponent->PrimaryComponentTick);
	}
}

void FParticleEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FParticleEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (bPendingClose)
	{
		Close();
		bPendingClose = false;
		return;
	}

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UParticleSystem* System = GetEditedSystem();

	bool bWindowOpen = true;
	FString VisibleTitle = "Particle Editor";
	if (System && !System->GetSourcePath().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += System->GetSourcePath();
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			bPendingClose = true;
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderToolbar();
	ImGui::Separator();

	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	const float TotalHeight = ImGui::GetContentRegionAvail().y;
	const float RightWidth = (std::max)(300.0f, TotalWidth - DetailsWidth - ImGui::GetStyle().ItemSpacing.x);
	const float TopHeight = (std::max)(160.0f, TotalHeight - CurvePanelHeight - ImGui::GetStyle().ItemSpacing.y);
	const float LeftWidth = (std::min)(DetailsWidth, TotalWidth * 0.45f);

	ImGui::BeginGroup();
	RenderPreviewViewport(ImVec2(LeftWidth, TopHeight * 0.58f));
	ImGui::Spacing();
	RenderPropertyPanel(ImVec2(LeftWidth, 0.0f));
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginGroup();
	RenderEmitterStrip(ImVec2(RightWidth, TopHeight));
	ImGui::Spacing();
	RenderCurveEditor(ImVec2(RightWidth, 0.0f));
	ImGui::EndGroup();

	ImGui::End();

	if (!bWindowOpen)
	{
		bPendingClose = true;
	}
}

void FParticleEditorWidget::RenderToolbar()
{
	UParticleSystem* System = GetEditedSystem();
	const bool bCanSave = System && !System->GetSourcePath().empty();

	if (!bCanSave) ImGui::BeginDisabled();
	if (ImGui::Button("Save"))
	{
		if (FParticleSystemManager::Get().Save(System))
		{
			ClearDirty();
		}
	}
	if (!bCanSave) ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Restart Sim"))
	{
		RestartPreview();
	}

	ImGui::SameLine();
	if (ImGui::Button(bSimPlaying ? "Pause" : "Play"))
	{
		bSimPlaying = !bSimPlaying;
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset Camera"))
	{
		ViewportClient.ResetCameraToPreviewBounds();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Bounds", &bShowBounds);
	ImGui::SameLine();
	ImGui::Checkbox("Origin Axis", &bShowOriginAxis);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::SliderFloat("Speed", &SimulationSpeed, 0.0f, 2.0f, "%.2fx"))
	{
		SimulationSpeed = (std::max)(0.0f, SimulationSpeed);
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(70.0f);
	if (ImGui::InputInt("LOD", &CurrentLODIndex))
	{
		if (CurrentLODIndex < 0) CurrentLODIndex = 0;
	}

	ImGui::SameLine();
	if (ImGui::Button("Select System"))
	{
		SelectSystem();
	}

	ImGui::SameLine();
	if (System)
	{
		ImGui::TextDisabled("Emitters: %d", System->GetEmitterCount());
	}
}

void FParticleEditorWidget::RenderPreviewViewport(ImVec2 Size)
{
	ImGui::BeginChild("##ParticlePreviewPanel", Size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 PanelSize = ImGui::GetContentRegionAvail();
	const ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, PanelSize.x, PanelSize.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (!VP || PanelSize.x <= 0.0f || PanelSize.y <= 0.0f)
	{
		ImGui::Dummy(PanelSize);
		ImGui::EndChild();
		return;
	}

	VP->RequestResize(static_cast<uint32>(PanelSize.x), static_cast<uint32>(PanelSize.y));
	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), PanelSize);
	}
	else
	{
		ImGui::Dummy(PanelSize);
	}

	FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ViewportPos, ImVec2(ViewportPos.x + PanelSize.x, ViewportPos.y + 28.0f), IM_COL32(30, 30, 30, 220));
	DrawList->AddText(ImVec2(ViewportPos.x + 8.0f, ViewportPos.y + 7.0f), IM_COL32(230, 230, 230, 255), "Viewport");

	const char* StateText = bSimPlaying ? "Sim: Playing" : "Sim: Paused";
	const ImVec2 StateSize = ImGui::CalcTextSize(StateText);
	DrawList->AddText(ImVec2(ViewportPos.x + PanelSize.x - StateSize.x - 8.0f, ViewportPos.y + 7.0f), IM_COL32(170, 220, 170, 255), StateText);

	if (bShowOriginAxis)
	{
		const ImVec2 Base(ViewportPos.x + 32.0f, ViewportPos.y + PanelSize.y - 28.0f);
		DrawList->AddLine(Base, ImVec2(Base.x + 24.0f, Base.y), IM_COL32(230, 80, 60, 255), 2.0f);
		DrawList->AddLine(Base, ImVec2(Base.x, Base.y - 24.0f), IM_COL32(80, 150, 255, 255), 2.0f);
		DrawList->AddText(ImVec2(Base.x + 27.0f, Base.y - 7.0f), IM_COL32(230, 80, 60, 255), "X");
		DrawList->AddText(ImVec2(Base.x - 5.0f, Base.y - 42.0f), IM_COL32(80, 150, 255, 255), "Z");
	}

	if (bShowBounds)
	{
		DrawList->AddRect(ImVec2(ViewportPos.x + 12.0f, ViewportPos.y + 42.0f),
			ImVec2(ViewportPos.x + PanelSize.x - 12.0f, ViewportPos.y + PanelSize.y - 12.0f),
			IM_COL32(220, 220, 100, 160), 0.0f, 0, 1.0f);
	}

	ImGui::EndChild();
}

void FParticleEditorWidget::RenderEmitterStrip(ImVec2 Size)
{
	ImGui::BeginChild("##ParticleEmitterStrip", Size, true, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::TextUnformatted("Emitters");
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Emitter"))
	{
		AddEmitter();
	}
	ImGui::SameLine();
	if (SelectedEmitterIndex < 0) ImGui::BeginDisabled();
	if (ImGui::SmallButton("- Emitter"))
	{
		RemoveSelectedEmitter();
	}
	if (SelectedEmitterIndex < 0) ImGui::EndDisabled();
	ImGui::SameLine();
	if (SelectedEmitterIndex < 0) ImGui::BeginDisabled();
	if (ImGui::SmallButton("+ Module"))
	{
		AddModuleToSelectedEmitter();
	}
	if (SelectedEmitterIndex < 0) ImGui::EndDisabled();
	ImGui::Separator();
	RenderAddModulePopup();

	UParticleSystem* System = GetEditedSystem();
	if (!System)
	{
		ImGui::TextDisabled("No particle system.");
		ImGui::EndChild();
		return;
	}

	if (System->Emitters.empty())
	{
		ImGui::TextDisabled("No emitters. Click + Emitter.");
		ImGui::EndChild();
		return;
	}

	for (int32 i = 0; i < static_cast<int32>(System->Emitters.size()); ++i)
	{
		RenderEmitterColumn(System->Emitters[i], i);
		if (i + 1 < static_cast<int32>(System->Emitters.size()))
		{
			ImGui::SameLine();
		}
	}

	ImGui::EndChild();
}

void FParticleEditorWidget::RenderEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	const float ColumnWidth = 178.0f;
	ImGui::PushID(EmitterIndex);
	ImGui::BeginGroup();

	const bool bSelectedEmitter = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == -1;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 HeaderPos = ImGui::GetCursorScreenPos();
	const ImVec2 HeaderSize(ColumnWidth, 54.0f);
	DrawList->AddRectFilled(HeaderPos, ImVec2(HeaderPos.x + HeaderSize.x, HeaderPos.y + HeaderSize.y),
		bSelectedEmitter ? IM_COL32(224, 119, 55, 255) : IM_COL32(58, 58, 62, 255), 2.0f);

	ImGui::InvisibleButton("##EmitterHeader", HeaderSize);
	if (ImGui::IsItemClicked())
	{
		SelectEmitter(EmitterIndex);
	}

	const char* Name = Emitter ? Emitter->EmitterName.c_str() : "Null Emitter";
	DrawList->AddText(ImVec2(HeaderPos.x + 8.0f, HeaderPos.y + 7.0f), IM_COL32(245, 245, 245, 255), Name);
	if (Emitter)
	{
		char CountText[64] = {};
		std::snprintf(CountText, sizeof(CountText), "LOD:%d  Modules:%zu", Emitter->GetLODCount(), Emitter->GetLODLevel(CurrentLODIndex) ? Emitter->GetLODLevel(CurrentLODIndex)->Modules.size() : 0);
		DrawList->AddText(ImVec2(HeaderPos.x + 8.0f, HeaderPos.y + 29.0f), IM_COL32(200, 200, 200, 255), CountText);
	}

	if (!Emitter)
	{
		ImGui::EndGroup();
		ImGui::PopID();
		return;
	}

	ImGui::PushItemWidth(ColumnWidth);
	bool bEnabled = Emitter->bEnabled;
	if (ImGui::Checkbox("Enabled", &bEnabled))
	{
		Emitter->bEnabled = bEnabled;
		NotifyParticleAssetChanged(true);
	}
	ImGui::PopItemWidth();

	UParticleLODLevel* LOD = Emitter->GetCurrentLODLevel(CurrentLODIndex);
	if (!LOD)
	{
		ImGui::TextDisabled("No LOD");
		ImGui::EndGroup();
		ImGui::PopID();
		return;
	}

	RenderModuleCard(LOD, LOD->RequiredModule, ModuleTokenRequired);
	RenderModuleCard(LOD, LOD->SpawnModule, ModuleTokenSpawn);
	if (LOD->TypeDataModule)
	{
		RenderModuleCard(LOD, LOD->TypeDataModule, ModuleTokenTypeData);
	}
	else
	{
		const bool bSelected = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == ModuleTokenTypeData;
		ImDrawList* LocalDrawList = ImGui::GetWindowDrawList();
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		LocalDrawList->AddRectFilled(Pos, ImVec2(Pos.x + ColumnWidth, Pos.y + 24.0f),
			bSelected ? IM_COL32(224, 119, 55, 255) : IM_COL32(35, 35, 38, 255));
		ImGui::InvisibleButton("##SpriteTypeData", ImVec2(ColumnWidth, 24.0f));
		if (ImGui::IsItemClicked())
		{
			SelectModule(EmitterIndex, ModuleTokenTypeData);
		}
		LocalDrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 5.0f), IM_COL32(180, 180, 180, 255), "TypeData: Sprite");
	}

	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(LOD->Modules.size()); ++ModuleIndex)
	{
		RenderModuleCard(LOD, LOD->Modules[ModuleIndex], ModuleIndex);
	}

	if (ImGui::BeginPopupContextItem("##EmitterContext"))
	{
		if (ImGui::MenuItem("Add Module"))
		{
			SelectEmitter(EmitterIndex);
			AddModuleToSelectedEmitter();
		}
		if (ImGui::MenuItem("Remove Emitter"))
		{
			SelectEmitter(EmitterIndex);
			RemoveSelectedEmitter();
		}
		ImGui::EndPopup();
	}

	ImGui::EndGroup();
	ImGui::PopID();
}

void FParticleEditorWidget::RenderModuleCard(UParticleLODLevel* LOD, UParticleModule* Module, int32 ModuleIndex)
{
	const float CardWidth = 178.0f;
	const float CardHeight = 24.0f;
	if (!Module)
	{
		ImGui::Dummy(ImVec2(CardWidth, CardHeight));
		return;
	}

	ImGui::PushID(ModuleIndex);
	int32 OwnerEmitterIndex = -1;
	if (LOD)
	{
		UParticleEmitter* OwnerEmitter = Cast<UParticleEmitter>(LOD->GetOuter());
		if (UParticleSystem* System = GetEditedSystem())
		{
			for (int32 i = 0; i < static_cast<int32>(System->Emitters.size()); ++i)
			{
				if (System->Emitters[i] == OwnerEmitter)
				{
					OwnerEmitterIndex = i;
					break;
				}
			}
		}
	}
	const bool bSelected = OwnerEmitterIndex == SelectedEmitterIndex && SelectedModuleIndex == ModuleIndex;
	const bool bEnabled = Module->IsEnabled();
	const ImVec2 Pos = ImGui::GetCursorScreenPos();
	const ImVec2 CheckMin(Pos.x + 6.0f, Pos.y + 5.0f);
	const ImVec2 CheckMax(CheckMin.x + 13.0f, CheckMin.y + 13.0f);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Pos, ImVec2(Pos.x + CardWidth, Pos.y + CardHeight),
		CategoryColor(Module->GetCategory(), bSelected, bEnabled));
	DrawList->AddRectFilled(CheckMin, CheckMax, IM_COL32(24, 24, 26, 255), 2.0f);
	DrawList->AddRect(CheckMin, CheckMax, IM_COL32(170, 170, 170, 255), 2.0f);
	if (bEnabled)
	{
		DrawList->AddLine(ImVec2(CheckMin.x + 3.0f, CheckMin.y + 7.0f), ImVec2(CheckMin.x + 6.0f, CheckMin.y + 10.0f), IM_COL32(235, 235, 235, 255), 2.0f);
		DrawList->AddLine(ImVec2(CheckMin.x + 6.0f, CheckMin.y + 10.0f), ImVec2(CheckMin.x + 11.0f, CheckMin.y + 3.0f), IM_COL32(235, 235, 235, 255), 2.0f);
	}
	DrawList->AddText(ImVec2(Pos.x + 28.0f, Pos.y + 5.0f), bEnabled ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 255), Module->GetDisplayName());

	ImGui::InvisibleButton("##ModuleCard", ImVec2(CardWidth, CardHeight));
	const bool bClickedEnabledToggle = ImGui::IsMouseHoveringRect(CheckMin, CheckMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	if (bClickedEnabledToggle)
	{
		Module->SetEnabled(!bEnabled);
		NotifyParticleAssetChanged(true);
	}
	else if (ImGui::IsItemClicked())
	{
		SelectModule(OwnerEmitterIndex, ModuleIndex);
	}

	if (ImGui::BeginPopupContextItem("##ModuleContext"))
	{
		if (ModuleIndex >= 0 && ImGui::MenuItem("Duplicate Module"))
		{
			SelectedEmitterIndex = OwnerEmitterIndex;
			SelectedModuleIndex = ModuleIndex;
			DuplicateSelectedModule();
		}
		if (ModuleIndex >= 0 && ImGui::MenuItem("Remove Module"))
		{
			SelectedEmitterIndex = OwnerEmitterIndex;
			SelectedModuleIndex = ModuleIndex;
			RemoveSelectedModule();
		}
		if (ModuleIndex == ModuleTokenTypeData && ImGui::MenuItem("Clear TypeData"))
		{
			SelectedEmitterIndex = OwnerEmitterIndex;
			SelectedModuleIndex = ModuleIndex;
			RemoveSelectedModule();
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
}

void FParticleEditorWidget::RenderPropertyPanel(ImVec2 Size)
{
	ImGui::BeginChild("##ParticleDetails", Size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::TextUnformatted("Details");
	ImGui::SameLine();
	ImGui::TextDisabled("%s", ModuleSelectionLabel(SelectedModuleIndex).c_str());
	ImGui::Separator();

	UParticleSystem* System = GetEditedSystem();
	if (!System)
	{
		ImGui::TextDisabled("No particle system.");
		ImGui::EndChild();
		return;
	}

	bool bChanged = false;
	bool bResetPreview = true;

	if (SelectedEmitterIndex < 0)
	{
		if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bChanged |= ImGui::Checkbox("Looping", &System->bLooping);
			bChanged |= ImGui::DragFloat("Update Time FPS", &System->UpdateTimeFPS, 1.0f, 10.0f, 120.0f, "%.0f");
			bChanged |= ImGui::Checkbox("Use Fixed Relative Bounding Box", &System->bUseFixedRelativeBoundingBox);
			bChanged |= DragFloat3Field("Bounds Min", System->SystemBoundsMin, 1.0f);
			bChanged |= DragFloat3Field("Bounds Max", System->SystemBoundsMax, 1.0f);
			ImGui::TextDisabled("Path: %s", System->GetSourcePath().empty() ? "Unsaved asset" : System->GetSourcePath().c_str());
		}
	}
	else if (UParticleEmitter* Emitter = GetSelectedEmitter())
	{
		UParticleLODLevel* LOD = GetSelectedLOD();
		UParticleModule* Module = GetSelectedModule();

		if (SelectedModuleIndex == -1)
		{
			if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bChanged |= InputTextFString("Emitter Name", Emitter->EmitterName);
				bChanged |= ImGui::Checkbox("Enabled", &Emitter->bEnabled);
				ImGui::Text("LOD Count: %d", Emitter->GetLODCount());
				ImGui::Text("Particle Size: %u bytes", Emitter->GetParticleSize());
				ImGui::Text("Instance Bytes: %u bytes", Emitter->GetReqInstanceBytes());
				if (ImGui::Button("Add LOD"))
				{
					Emitter->CreateLODLevel(Emitter->GetLODCount());
					NotifyParticleAssetChanged(true);
				}
				ImGui::SameLine();
				if (Emitter->GetLODCount() <= 1) ImGui::BeginDisabled();
				if (ImGui::Button("Remove Current LOD"))
				{
					Emitter->RemoveLODLevel(CurrentLODIndex);
					if (CurrentLODIndex >= Emitter->GetLODCount()) CurrentLODIndex = (std::max)(0, Emitter->GetLODCount() - 1);
					NotifyParticleAssetChanged(true);
				}
				if (Emitter->GetLODCount() <= 1) ImGui::EndDisabled();
			}

			if (LOD && ImGui::CollapsingHeader("Current LOD", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Level: %d", LOD->Level);
				bChanged |= ImGui::Checkbox("LOD Enabled", &LOD->bEnabled);
			}
		}
		else if (SelectedModuleIndex == ModuleTokenTypeData && !Module)
		{
			ImGui::TextWrapped("Sprite emitter: TypeData module is empty. Add Mesh/Beam/Ribbon TypeData from Add Module menu to switch emitter type.");
			if (ImGui::Button("Add TypeData Mesh"))
			{
				if (LOD)
				{
					UParticleModuleTypeDataMesh* NewModule = CreateParticleModule<UParticleModuleTypeDataMesh>(LOD, Emitter);
					if (LOD->AddModule(NewModule))
					{
						NotifyParticleAssetChanged(true);
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Beam"))
			{
				if (LOD)
				{
					UParticleModuleTypeDataBeam* NewModule = CreateParticleModule<UParticleModuleTypeDataBeam>(LOD, Emitter);
					if (LOD->AddModule(NewModule)) NotifyParticleAssetChanged(true);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Ribbon"))
			{
				if (LOD)
				{
					UParticleModuleTypeDataRibbon* NewModule = CreateParticleModule<UParticleModuleTypeDataRibbon>(LOD, Emitter);
					if (LOD->AddModule(NewModule)) NotifyParticleAssetChanged(true);
				}
			}
		}
		else if (Module)
		{
			ImGui::Text("Module: %s", Module->GetDisplayName());
			ImGui::TextDisabled("Category: %s", CategoryName(Module->GetCategory()));
			bool bModuleEnabled = Module->IsEnabled();
			if (ImGui::Checkbox("Module Enabled", &bModuleEnabled))
			{
				Module->SetEnabled(bModuleEnabled);
				bChanged = true;
			}

			if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
			{
				if (ImGui::CollapsingHeader("Required", ImGuiTreeNodeFlags_DefaultOpen))
				{
					const FString ForcedShader = ParticleForcedShaderPath(LOD);
					if (MaterialComboFieldFiltered("Material", Required->MaterialSlot, ForcedShader))
					{
						Required->CachedMaterial = nullptr;
						bChanged = true;
					}
					// 셰이더는 emitter 타입이 강제 → 그 셰이더 레이아웃과 맞는 머티리얼만 위 목록에 표시.
					ImGui::TextDisabled("Shader (forced by emitter): %s", ForcedShader.c_str());
					// Blend State는 Material(.mat)이 결정 — 에디터 비노출. Material 슬롯에서 변경한다.
					ImGui::TextDisabled("Blend State: from Material (.mat)");
					bChanged |= ImGui::Checkbox("Use Local Space", &Required->bUseLocalSpace);
					bChanged |= ImGui::DragInt("SubImages Horizontal", &Required->SubImagesHorizontal, 1.0f, 1, 64);
					bChanged |= ImGui::DragInt("SubImages Vertical", &Required->SubImagesVertical, 1.0f, 1, 64);
					bChanged |= ImGui::DragFloat("Emitter Duration", &Required->EmitterDuration, 0.05f, 0.0f, 9999.0f, "%.2f");
					bChanged |= ImGui::DragInt("Emitter Loops", &Required->EmitterLoops, 1.0f, 0, 9999);
					static const char* SortNames[] = { "None", "ViewProjDepth", "ViewDistance", "Age_OldestFirst", "Age_NewestFirst" };
					int32 Sort = static_cast<int32>(Required->SortMode);
					if (ComboInt("Sort Mode", Sort, SortNames, 5)) { Required->SortMode = static_cast<UParticleModuleRequired::ESortMode>(Sort); bChanged = true; }
					static const char* AlignNames[] = { "Square", "Rectangle", "Velocity", "FacingCameraPosition" };
					int32 Align = static_cast<int32>(Required->ScreenAlignment);
					if (ComboInt("Screen Alignment", Align, AlignNames, 4)) { Required->ScreenAlignment = static_cast<UParticleModuleRequired::EScreenAlignment>(Align); bChanged = true; }
				}
			}
			else if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
			{
				if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= ImGui::DragFloat("Rate (particles/sec)", &Spawn->Rate, 1.0f, 0.0f, 10000.0f, "%.1f");
					bChanged |= ImGui::DragFloat("Rate Scale", &Spawn->RateScale, 0.01f, 0.0f, 10.0f, "%.2f");
					if (ImGui::TreeNodeEx("Bursts", ImGuiTreeNodeFlags_DefaultOpen))
					{
						for (int32 i = 0; i < static_cast<int32>(Spawn->BurstList.size()); ++i)
						{
							ImGui::PushID(i);
							ImGui::Separator();
							ImGui::Text("Burst %d", i);
							bChanged |= ImGui::DragFloat("Time", &Spawn->BurstList[i].Time, 0.01f, 0.0f, 9999.0f, "%.2f");
							bChanged |= ImGui::DragInt("Count", &Spawn->BurstList[i].Count, 1.0f, 0, 100000);
							if (ImGui::SmallButton("Remove Burst"))
							{
								Spawn->BurstList.erase(Spawn->BurstList.begin() + i);
								bChanged = true;
								ImGui::PopID();
								break;
							}
							ImGui::PopID();
						}
						if (ImGui::Button("Add Burst"))
						{
							UParticleModuleSpawn::FBurstEntry Entry;
							Entry.Time = 0.0f;
							Entry.Count = 10;
							Entry.bFired = false;
							Spawn->BurstList.push_back(Entry);
							bChanged = true;
						}
						ImGui::TreePop();
					}
				}
			}
			else if (UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
			{
				if (ImGui::CollapsingHeader("Lifetime", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= ImGui::DragFloat("Min Lifetime", &Lifetime->MinLifetime, 0.05f, 0.001f, 60.0f, "%.2f");
					bChanged |= ImGui::DragFloat("Max Lifetime", &Lifetime->MaxLifetime, 0.05f, 0.001f, 60.0f, "%.2f");
				}
			}
			else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
			{
				if (ImGui::CollapsingHeader("Location", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= DragFloat3Field("Start Location Min", Location->StartLocationMin, 0.1f);
					bChanged |= DragFloat3Field("Start Location Max", Location->StartLocationMax, 0.1f);
					bChanged |= ImGui::Checkbox("World Space Override", &Location->bWorldSpaceOverride);
				}
			}
			else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
			{
				if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= DragFloat3Field("Start Velocity Min", Velocity->StartVelocityMin, 0.1f);
					bChanged |= DragFloat3Field("Start Velocity Max", Velocity->StartVelocityMax, 0.1f);
					bChanged |= ImGui::Checkbox("In World Space", &Velocity->bInWorldSpace);
				}
			}
			else if (UParticleModuleAcceleration* Acceleration = Cast<UParticleModuleAcceleration>(Module))
			{
				if (ImGui::CollapsingHeader("Const Acceleration", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= DragFloat3Field("Acceleration", Acceleration->Acceleration, 0.1f);
				}
			}
			else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
			{
				if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= Color4Field("Start Color", Color->StartColor);
					bChanged |= Color4Field("End Color", Color->EndColor);
				}
			}
			else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
			{
				if (ImGui::CollapsingHeader("Size", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= DragFloat3Field("Start Size Min", Size->StartSizeMin, 0.1f, 0.0f, 10000.0f);
					bChanged |= DragFloat3Field("Start Size Max", Size->StartSizeMax, 0.1f, 0.0f, 10000.0f);
					bChanged |= DragFloat3Field("End Size Scale", Size->EndSizeScale, 0.01f, 0.0f, 10000.0f);
					bChanged |= ImGui::Checkbox("Animate Over Life", &Size->bAnimateOverLife);
				}
			}
			else if (UParticleModuleCollision* Collision = Cast<UParticleModuleCollision>(Module))
			{
				if (ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= ImGui::DragFloat("Damping Factor", &Collision->DampingFactor, 0.01f, 0.0f, 1.0f, "%.2f");
					bChanged |= ImGui::DragInt("Max Collisions", &Collision->MaxCollisions, 1.0f, 0, 1000);
					static const char* ChannelNames[] = { "WorldStatic", "WorldDynamic", "Pawn", "Projectile", "Trigger" };
					int32 Channel = static_cast<int32>(Collision->CollisionChannel);
					if (ComboInt("Collision Channel", Channel, ChannelNames, 5)) { Collision->CollisionChannel = static_cast<ECollisionChannel>(Channel); bChanged = true; }
					bChanged |= ImGui::Checkbox("Kill On Collision", &Collision->bKillOnCollision);
					bChanged |= ImGui::Checkbox("Generate Collision Events", &Collision->bGenerateCollisionEvents);
				}
			}
			else if (UParticleModuleEventGenerator* EventGen = Cast<UParticleModuleEventGenerator>(Module))
			{
				if (ImGui::CollapsingHeader("Event Generator", ImGuiTreeNodeFlags_DefaultOpen))
				{
					static const char* EventNames[] = { "Spawn", "Death", "Collision", "Burst" };
					for (int32 i = 0; i < static_cast<int32>(EventGen->Entries.size()); ++i)
					{
						ImGui::PushID(i);
						ImGui::Separator();
						ImGui::Text("Entry %d", i);
						int32 Type = static_cast<int32>(EventGen->Entries[i].Type);
						if (ComboInt("Type", Type, EventNames, 4)) { EventGen->Entries[i].Type = static_cast<EParticleEventType>(Type); bChanged = true; }
						FString EventName = EventGen->Entries[i].EventName.ToString();
						if (InputTextFString("Event Name", EventName)) { EventGen->Entries[i].EventName = FName(EventName); bChanged = true; }
						bChanged |= ImGui::Checkbox("Enabled", &EventGen->Entries[i].bEnabled);
						if (ImGui::SmallButton("Remove Entry"))
						{
							EventGen->Entries.erase(EventGen->Entries.begin() + i);
							bChanged = true;
							ImGui::PopID();
							break;
						}
						ImGui::PopID();
					}
					if (ImGui::Button("Add Event Entry"))
					{
						UParticleModuleEventGenerator::FEntry Entry;
						Entry.Type = EParticleEventType::Death;
						Entry.EventName = FName("ParticleEvent");
						Entry.bEnabled = true;
						EventGen->Entries.push_back(Entry);
						bChanged = true;
					}
				}
			}
			else if (UParticleModuleTypeDataMesh* Mesh = Cast<UParticleModuleTypeDataMesh>(Module))
			{
				if (ImGui::CollapsingHeader("TypeData Mesh", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (MeshComboField("Static Mesh", Mesh->MeshSlot))
					{
						Mesh->CachedMesh = nullptr;
						bChanged = true;
					}
					static const char* AlignmentNames[] = { "None", "Velocity", "FacingCamera", "AxisLock" };
					int32 Alignment = static_cast<int32>(Mesh->Alignment);
					if (ComboInt("Alignment", Alignment, AlignmentNames, 4)) { Mesh->Alignment = static_cast<UParticleModuleTypeDataMesh::EMeshAlignment>(Alignment); bChanged = true; }
					bChanged |= ImGui::Checkbox("Override Material", &Mesh->bOverrideMaterial);
				}
			}
			else if (UParticleModuleTypeDataBeam* Beam = Cast<UParticleModuleTypeDataBeam>(Module))
			{
				if (ImGui::CollapsingHeader("TypeData Beam", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= ImGui::DragInt("Interpolation Points", &Beam->InterpolationPoints, 1.0f, 0, 128);
					bChanged |= ImGui::DragFloat("Noise Amount", &Beam->NoiseAmount, 0.1f, 0.0f, 100.0f, "%.2f");
					bChanged |= ImGui::DragFloat("Noise Frequency", &Beam->NoiseFrequency, 0.1f, 0.0f, 20.0f, "%.2f");
					bChanged |= ImGui::Checkbox("Tile UV", &Beam->bTileUV);
					bChanged |= DragFloat3Field("Default Source", Beam->DefaultSource, 0.1f);
					bChanged |= DragFloat3Field("Default Target", Beam->DefaultTarget, 0.1f);
				}
			}
			else if (UParticleModuleTypeDataRibbon* Ribbon = Cast<UParticleModuleTypeDataRibbon>(Module))
			{
				if (ImGui::CollapsingHeader("TypeData Ribbon", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bChanged |= ImGui::DragInt("Max Tessellation", &Ribbon->MaxTessellation, 1.0f, 1, 64);
					bChanged |= ImGui::DragFloat("Tangent Tension", &Ribbon->TangentTension, 0.01f, 0.0f, 1.0f, "%.2f");
					bChanged |= ImGui::DragFloat("Tiles Per Trail", &Ribbon->TilesPerTrail, 0.01f, 0.0f, 9999.0f, "%.2f");
				}
			}
			else
			{
				ImGui::TextWrapped("This module has no dedicated editor yet. It is still selectable and can be enabled/disabled/removed if it is not a core module.");
				bResetPreview = false;
			}

			if (SelectedModuleIndex >= 0 || SelectedModuleIndex == ModuleTokenTypeData)
			{
				ImGui::Separator();
				if (SelectedModuleIndex == ModuleTokenTypeData && !Module) ImGui::BeginDisabled();
				if (SelectedModuleIndex == ModuleTokenTypeData || SelectedModuleIndex >= 0)
				{
					if (ImGui::Button(SelectedModuleIndex == ModuleTokenTypeData ? "Clear TypeData" : "Remove Module"))
					{
						RemoveSelectedModule();
					}
				}
				if (SelectedModuleIndex == ModuleTokenTypeData && !Module) ImGui::EndDisabled();
			}
		}
	}

	if (bChanged)
	{
		NotifyParticleAssetChanged(bResetPreview);
	}

	ImGui::EndChild();
}

void FParticleEditorWidget::RenderCurveEditor(ImVec2 Size)
{
	ImGui::BeginChild("##ParticleCurveEditor", Size, true);
	ImGui::TextUnformatted("Curve Editor");
	ImGui::SameLine();
	ImGui::TextDisabled("(Distribution/FloatCurve backend not connected yet)");
	ImGui::Separator();

	const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(45, 45, 45, 255));

	const float GridX = 64.0f;
	const float GridY = 32.0f;
	for (float x = CanvasPos.x; x < CanvasPos.x + CanvasSize.x; x += GridX)
	{
		DrawList->AddLine(ImVec2(x, CanvasPos.y), ImVec2(x, CanvasPos.y + CanvasSize.y), IM_COL32(120, 120, 120, 100));
	}
	for (float y = CanvasPos.y; y < CanvasPos.y + CanvasSize.y; y += GridY)
	{
		DrawList->AddLine(ImVec2(CanvasPos.x, y), ImVec2(CanvasPos.x + CanvasSize.x, y), IM_COL32(120, 120, 120, 100));
	}

	DrawList->AddText(ImVec2(CanvasPos.x + 10.0f, CanvasPos.y + 10.0f), IM_COL32(190, 190, 190, 255),
		"Modules can be sent here after curve/distribution support is added.");
	ImGui::Dummy(CanvasSize);
	ImGui::EndChild();
}

void FParticleEditorWidget::AddEmitter()
{
	UParticleSystem* System = GetEditedSystem();
	if (!System) return;

	UParticleEmitter* NewEmitter = System->AddEmitter();
	if (!NewEmitter) return;

	SelectedEmitterIndex = static_cast<int32>(System->Emitters.size()) - 1;
	SelectedModuleIndex = -1;
	NotifyParticleAssetChanged(true);
}

void FParticleEditorWidget::RemoveSelectedEmitter()
{
	UParticleSystem* System = GetEditedSystem();
	if (!System || SelectedEmitterIndex < 0) return;

	System->RemoveEmitter(SelectedEmitterIndex);
	if (SelectedEmitterIndex >= static_cast<int32>(System->Emitters.size()))
	{
		SelectedEmitterIndex = static_cast<int32>(System->Emitters.size()) - 1;
	}
	SelectedModuleIndex = -1;
	if (System->Emitters.empty())
	{
		SelectSystem();
	}
	NotifyParticleAssetChanged(true);
}

void FParticleEditorWidget::AddModuleToSelectedEmitter()
{
	ImGui::OpenPopup("Add Particle Module");
}

void FParticleEditorWidget::RenderAddModulePopup()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	UParticleLODLevel* LOD = GetSelectedLOD();

	if (ImGui::BeginPopup("Add Particle Module"))
	{
		if (!Emitter || !LOD)
		{
			ImGui::TextDisabled("Select an emitter first.");
			ImGui::EndPopup();
			return;
		}

		auto AddRegular = [&](const char* Label, UParticleModule::EModuleCategory Category, auto Creator)
		{
			const bool bExists = HasCategory(LOD, Category);
			if (bExists) ImGui::BeginDisabled();
			if (ImGui::MenuItem(Label))
			{
				UParticleModule* Module = Creator();
				if (Module && LOD->AddModule(Module))
				{
					SelectedModuleIndex = static_cast<int32>(LOD->Modules.size()) - 1;
					NotifyParticleAssetChanged(true);
				}
				else if (Module)
				{
					UObjectManager::Get().DestroyObject(Module);
				}
			}
			if (bExists) ImGui::EndDisabled();
		};

		AddRegular("Lifetime", UParticleModule::EModuleCategory::Lifetime, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleLifetime>(LOD, Emitter); });
		AddRegular("Initial Location", UParticleModule::EModuleCategory::Location, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleLocation>(LOD, Emitter); });
		AddRegular("Initial Velocity", UParticleModule::EModuleCategory::Velocity, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleVelocity>(LOD, Emitter); });
		AddRegular("Const Acceleration", UParticleModule::EModuleCategory::Acceleration, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleAcceleration>(LOD, Emitter); });
		AddRegular("Color", UParticleModule::EModuleCategory::Color, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleColor>(LOD, Emitter); });
		AddRegular("Size", UParticleModule::EModuleCategory::Size, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleSize>(LOD, Emitter); });
		AddRegular("Collision", UParticleModule::EModuleCategory::Collision, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleCollision>(LOD, Emitter); });
		AddRegular("Event Generator", UParticleModule::EModuleCategory::Event, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleEventGenerator>(LOD, Emitter); });
		AddRegular("Sub Image Index", UParticleModule::EModuleCategory::SubUV, [&]() -> UParticleModule* { return CreateParticleModule<UParticleModuleSubUV>(LOD, Emitter); });

		ImGui::Separator();
		const bool bHasTypeData = LOD->TypeDataModule != nullptr;
		if (bHasTypeData) ImGui::BeginDisabled();
		if (ImGui::MenuItem("TypeData Mesh"))
		{
			UParticleModule* Module = CreateParticleModule<UParticleModuleTypeDataMesh>(LOD, Emitter);
			if (Module && LOD->AddModule(Module))
			{
				SelectedModuleIndex = ModuleTokenTypeData;
				NotifyParticleAssetChanged(true);
			}
			else if (Module)
			{
				UObjectManager::Get().DestroyObject(Module);
			}
		}
		if (ImGui::MenuItem("TypeData Beam"))
		{
			UParticleModule* Module = CreateParticleModule<UParticleModuleTypeDataBeam>(LOD, Emitter);
			if (Module && LOD->AddModule(Module))
			{
				SelectedModuleIndex = ModuleTokenTypeData;
				NotifyParticleAssetChanged(true);
			}
			else if (Module)
			{
				UObjectManager::Get().DestroyObject(Module);
			}
		}
		if (ImGui::MenuItem("TypeData Ribbon"))
		{
			UParticleModule* Module = CreateParticleModule<UParticleModuleTypeDataRibbon>(LOD, Emitter);
			if (Module && LOD->AddModule(Module))
			{
				SelectedModuleIndex = ModuleTokenTypeData;
				NotifyParticleAssetChanged(true);
			}
			else if (Module)
			{
				UObjectManager::Get().DestroyObject(Module);
			}
		}
		if (bHasTypeData) ImGui::EndDisabled();

		ImGui::EndPopup();
	}
}

void FParticleEditorWidget::DuplicateSelectedModule()
{
	UParticleLODLevel* LOD = GetSelectedLOD();
	UParticleModule* Module = GetSelectedModule();
	if (!LOD || !Module || SelectedModuleIndex < 0)
	{
		return;
	}

	UObject* DuplicateObject = Module->Duplicate(LOD);
	UParticleModule* DuplicateModule = Cast<UParticleModule>(DuplicateObject);
	if (!DuplicateModule)
	{
		return;
	}

	if (LOD->AddModule(DuplicateModule))
	{
		SelectedModuleIndex = static_cast<int32>(LOD->Modules.size()) - 1;
		NotifyParticleAssetChanged(true);
	}
	else
	{
		UObjectManager::Get().DestroyObject(DuplicateModule);
	}
}

void FParticleEditorWidget::RemoveSelectedModule()
{
	UParticleLODLevel* LOD = GetSelectedLOD();
	UParticleModule* Module = GetSelectedModule();
	if (!LOD)
	{
		return;
	}

	if (SelectedModuleIndex == ModuleTokenTypeData && LOD->TypeDataModule)
	{
		Module = LOD->TypeDataModule;
	}

	if (!Module)
	{
		return;
	}

	if (LOD->RemoveModule(Module))
	{
		if (SelectedModuleIndex >= static_cast<int32>(LOD->Modules.size()))
		{
			SelectedModuleIndex = static_cast<int32>(LOD->Modules.size()) - 1;
		}
		if (SelectedModuleIndex < 0 && SelectedModuleIndex != ModuleTokenTypeData)
		{
			SelectedModuleIndex = -1;
		}
		NotifyParticleAssetChanged(true);
	}
}

void FParticleEditorWidget::SelectSystem()
{
	SelectedEmitterIndex = -1;
	SelectedModuleIndex = -1;
}

void FParticleEditorWidget::SelectEmitter(int32 EmitterIndex)
{
	SelectedEmitterIndex = EmitterIndex;
	SelectedModuleIndex = -1;
}

void FParticleEditorWidget::SelectModule(int32 EmitterIndex, int32 ModuleIndex)
{
	SelectedEmitterIndex = EmitterIndex;
	SelectedModuleIndex = ModuleIndex;
}

void FParticleEditorWidget::RebuildPreview(bool bResetSimulation)
{
	UParticleSystem* System = GetEditedSystem();
	if (System)
	{
		System->BuildEmitters();
	}

	if (PreviewParticleComponent)
	{
		PreviewParticleComponent->SetTemplate(System);
		PreviewParticleComponent->RebuildInstances(bResetSimulation);
		PreviewParticleComponent->Activate(bResetSimulation);
	}
}

void FParticleEditorWidget::RestartPreview()
{
	if (PreviewParticleComponent)
	{
		PreviewParticleComponent->Activate(true);
		PreviewParticleComponent->ResetParticles();
	}
}

void FParticleEditorWidget::NotifyParticleAssetChanged(bool bResetSimulation)
{
	MarkDirty();
	RebuildPreview(bResetSimulation);
}

UParticleEmitter* FParticleEditorWidget::GetSelectedEmitter() const
{
	UParticleSystem* System = GetEditedSystem();
	if (!System) return nullptr;
	return System->GetEmitter(SelectedEmitterIndex);
}

UParticleLODLevel* FParticleEditorWidget::GetSelectedLOD() const
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter) return nullptr;
	return Emitter->GetCurrentLODLevel(CurrentLODIndex);
}

UParticleModule* FParticleEditorWidget::GetSelectedModule() const
{
	UParticleLODLevel* LOD = GetSelectedLOD();
	if (!LOD) return nullptr;

	switch (SelectedModuleIndex)
	{
	case ModuleTokenRequired: return LOD->RequiredModule;
	case ModuleTokenSpawn:    return LOD->SpawnModule;
	case ModuleTokenTypeData: return LOD->TypeDataModule;
	default:
		break;
	}

	if (SelectedModuleIndex >= 0 && SelectedModuleIndex < static_cast<int32>(LOD->Modules.size()))
	{
		return LOD->Modules[SelectedModuleIndex];
	}
	return nullptr;
}

UParticleSystem* FParticleEditorWidget::GetEditedSystem() const
{
	return Cast<UParticleSystem>(EditedObject);
}
