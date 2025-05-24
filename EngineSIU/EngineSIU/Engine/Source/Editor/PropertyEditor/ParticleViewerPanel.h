#pragma once
#include "Engine/EditorEngine.h"
//#include "Particles/ParticleEmitter.h"
//#include "Particles/ParticleModule.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Color/ParticleModuleColorBase.h"
#include "Particles/Location/ParticleModuleLocationBase.h"
#include "Particles/Size/ParticleModuleSizeBase.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/SubUV/ParticleModuleSUbUVBase.h"
#include "Particles/Velocity/ParticleModuleVelocityBase.h"
#include "UnrealEd/EditorPanel.h"

enum class EDynamicEmitterType : uint8;
class UParticleSystemComponent;
class UParticleSystem;
class UParticleEmitter;
class UParticleModule;

class ParticleViewerPanel : public UEditorPanel
{
public:
    ParticleViewerPanel();

    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;

    void SetParticleSystem(UParticleSystem* InParticleSystem)
    { 
        ParticleSystem = InParticleSystem; 
    }
    void SetParticleSystemComponent(UParticleSystemComponent* InParticleSystemComponent)
    {
        ParticleSystemComponent = InParticleSystemComponent;
    }

private:
    TArray<UClass*> DisAddableClasses = {
        UParticleModule::StaticClass(),
        UParticleModuleRequired::StaticClass(),
        UParticleModuleSpawn::StaticClass(),
        UParticleModuleSpawnBase::StaticClass(),
        UParticleModuleColorBase::StaticClass(),
        UParticleModuleSizeBase::StaticClass(),
        UParticleModuleVelocityBase::StaticClass(),
        UParticleModuleLocationBase::StaticClass(),
        UParticleModuleSUbUVBase::StaticClass(),
    };

    TArray<UClass*> DisDeletableClasses = {
        UParticleModuleRequired::StaticClass(),
        UParticleModuleSpawn::StaticClass()
    };
    
    float Width = 0, Height = 0;
    UParticleSystemComponent* ParticleSystemComponent = nullptr;
    UParticleSystem* ParticleSystem = nullptr;
    UParticleEmitter* SelectedEmitter = nullptr;
    UParticleModule* SelectedModule = nullptr;
    
    float DetailSize = 1.0f;
    float DetailSpeed = 5.0f;
    float DetailLife = 10.0f;
    float DetailColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float DetailGravity = 0.0f;
    float DetailWind = 0.0f;
    float DetailTurbulence = 0.0f;
    float DetailBounce = 0.5f;
    float DetailAlphaCutoff = 0.5f;
    int DetailBlendMode = 0;
    bool DetailCastShadows = false;
    bool DetailReceiveShadows = true;

    float ControlHeightSize = 0.05f;
    float ToolHeightSize = 0.05f;
    float ViewHeightSize = 0.4f;
    float DetailHeightSize = 0.45f;
    
    float LeftWidthSize = 0.4f;

    float TopMargin = 5.0f;
    float LeftMargin = 5.0f;
    
    void RenderControlPanel() const;
    void RenderToolbarPanel() const;
    void RenderEmitterPanel();
    void AddNewEmitter(EDynamicEmitterType Type);
    void RenderDetailPanel();
    void RenderEffectSet(UParticleEmitter* Emitter);
    void RenderExitButton() const;
};
