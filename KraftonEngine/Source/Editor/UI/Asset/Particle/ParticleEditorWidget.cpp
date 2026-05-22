#include "ParticleEditorWidget.h"

#include <imgui.h>

#include "Particle/ParticleSystem.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"

FParticleEditorWidget::FParticleEditorWidget() {}
FParticleEditorWidget::~FParticleEditorWidget() {}

bool FParticleEditorWidget::CanEdit(UObject* Object) const
{
	return Cast<UParticleSystem>(Object) != nullptr;
}

bool FParticleEditorWidget::IsEditingObject(UObject* Object) const
{
	return IsOpen() && EditedObject == Object;
}

void FParticleEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	SelectedEmitterIndex = -1;
	SelectedModuleIndex  = -1;
}

void FParticleEditorWidget::Close()
{
	FAssetEditorWidget::Close();
}

void FParticleEditorWidget::Tick(float DeltaTime) {}
void FParticleEditorWidget::Render(float DeltaTime)
{
	// TODO: ImGui Begin → RenderToolbar → split (Strip + Property | Preview) → optional CurveEditor.
}

void FParticleEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const {}

void FParticleEditorWidget::RenderToolbar()                                    {}
void FParticleEditorWidget::RenderEmitterStrip(ImVec2 Size)                   {}
void FParticleEditorWidget::RenderPropertyPanel(ImVec2 Size)                  {}
void FParticleEditorWidget::RenderPreviewViewport(ImVec2 Size)                {}
void FParticleEditorWidget::RenderCurveEditor(ImVec2 Size)                    {}

void FParticleEditorWidget::RenderEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex) {}
void FParticleEditorWidget::RenderModuleCard(UParticleLODLevel* LOD, UParticleModule* Module, int32 ModuleIndex) {}

void FParticleEditorWidget::AddEmitter()                                       {}
void FParticleEditorWidget::RemoveSelectedEmitter()                            {}
void FParticleEditorWidget::AddModuleToSelectedEmitter()                       {}
void FParticleEditorWidget::DuplicateSelectedModule()                          {}
void FParticleEditorWidget::RemoveSelectedModule()                             {}

UParticleSystem* FParticleEditorWidget::GetEditedSystem() const
{
	return Cast<UParticleSystem>(EditedObject);
}
