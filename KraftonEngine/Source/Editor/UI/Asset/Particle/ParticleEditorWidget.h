#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

class UParticleSystem;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class IEditorPreviewViewportClient;
struct ImVec2;

// =============================================================================
// FParticleEditorWidget (Cascade)
//   UParticleSystem 에셋 편집기.
//   레이아웃 (좌→우):
//     [Emitter Strip] : emitter 1개당 세로 열, 위→아래로 Required / TypeData /
//                       Spawn / 기타 module 슬롯이 카드처럼 쌓인다.
//     [Property Panel]: 선택된 module 의 UPROPERTY 편집 UI.
//     [Preview Viewport] : PSC 1개를 띄운 별도 viewport 에 입자 재생.
//     [Curve Editor (옵션)]: distribution/curve 편집 (선택 사항이라 인터페이스만).
// =============================================================================
class FParticleEditorWidget : public FAssetEditorWidget
{
public:
	FParticleEditorWidget();
	~FParticleEditorWidget() override;

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;
	bool AllowsMultipleInstances() const override { return true; }

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

private:
	// 패널 단위 렌더
	void RenderToolbar();
	void RenderEmitterStrip(ImVec2 Size);
	void RenderPropertyPanel(ImVec2 Size);
	void RenderPreviewViewport(ImVec2 Size);
	void RenderCurveEditor(ImVec2 Size); // 옵션 — stub

	// emitter strip 의 1 column 렌더
	void RenderEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex);
	void RenderModuleCard(UParticleLODLevel* LOD, UParticleModule* Module, int32 ModuleIndex);

	// 토큰 작업 (메뉴/단축키)
	void AddEmitter();
	void RemoveSelectedEmitter();
	void AddModuleToSelectedEmitter(/* category */);
	void DuplicateSelectedModule();
	void RemoveSelectedModule();

	// 선택 상태
	UParticleSystem* GetEditedSystem() const;

private:
	int32 SelectedEmitterIndex = -1;
	int32 SelectedModuleIndex  = -1; // LOD0.Modules 인덱스 (Required/Spawn/TypeData 은 음수 토큰)
	int32 CurrentLODIndex      = 0;
};
