#include "ParticleSystemComponent.h"
#include "ParticleEmitterInstance.h"
#include "LevelEditor/SLevelEditor.h"
#include "Particles/ParticleSystem.h"
#include "UnrealEd/EditorViewportClient.h"
#include "ParticleEmitter.h"

UParticleSystemComponent::UParticleSystemComponent()
    : AccumTickTime(0.f)
    , Template(nullptr)
{
}

UObject* UParticleSystemComponent::Duplicate(UObject* InOuter)
{
    return nullptr;
}

void UParticleSystemComponent::InitializeComponent()
{
    Super::InitializeComponent();

    if (Template)
    {
        InitializeSystem();
    }
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!Template)
    {
        return;
    }

    if (EmitterInstances.Num() != Template->GetEmitters().Num())
    {
        EmitterInstances.Empty();
        InitializeSystem();
    }

    for (auto* Instance : EmitterInstances)
    {
        if (Instance)
        {
            Instance->Tick(DeltaTime);
        }
    }

    UpdateDynamicData();
}

void UParticleSystemComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
}

void UParticleSystemComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
}

void UParticleSystemComponent::InitializeSystem()
{
    TArray<UParticleEmitter*> Emitters = Template->GetEmitters();
    for (int32 i = 0; i < Emitters.Num(); i++)
    {
        UParticleEmitter* EmitterTemplate = Emitters[i];
        if (EmitterTemplate->EmitterType == EDynamicEmitterType::DET_Sprite)
        {
            CreateAndAddSpriteEmitterInstance(EmitterTemplate);
        }
        else if (EmitterTemplate->EmitterType == EDynamicEmitterType::DET_Mesh)
        {
            CreateAndAddMeshEmitterInstance(EmitterTemplate);
        }
    }
}

void UParticleSystemComponent::CreateAndAddSpriteEmitterInstance(UParticleEmitter* EmitterTemplate)
{
    if (EmitterTemplate)
    {
        FParticleSpriteEmitterInstance* Instance = new FParticleSpriteEmitterInstance();
        Instance->SpriteTemplate = EmitterTemplate;
        Instance->Component = this;
        Instance->CurrentLODLevelIndex = 0;

        Instance->Initialize();

        EmitterInstances.Add(Instance);
    }
}

void UParticleSystemComponent::CreateAndAddMeshEmitterInstance(UParticleEmitter* EmitterTemplate)
{
    if (EmitterTemplate)
    {
        FParticleMeshEmitterInstance* Instance = new FParticleMeshEmitterInstance();
        Instance->SpriteTemplate = EmitterTemplate;
        Instance->Component = this;
        Instance->CurrentLODLevelIndex = 0;

        Instance->Initialize();

        EmitterInstances.Add(Instance);
    }
}

void UParticleSystemComponent::UpdateDynamicData()
{
    // Create the dynamic data for rendering this particle system
    ParticleDynamicData = CreateDynamicData();
}

FParticleDynamicData* UParticleSystemComponent::CreateDynamicData()
{
    if (EmitterInstances.Num() > 0)
    {
        int32 LiveCount = 0;
        for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
        {
            FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
            if (EmitInst)
            {
                if (EmitInst->ActiveParticles > 0)
                {
                    LiveCount++;
                }
            }
        }

        if (LiveCount == 0)
        {
            return nullptr;
        }
    }

    FParticleDynamicData* ParticleDynamicData = new FParticleDynamicData();

    if (Template)
    {
        ParticleDynamicData->SystemPositionForMacroUVs = GetComponentTransform().TransformPosition(Template->GetMacroUVPosition());
        ParticleDynamicData->SystemRadiusForMacroUVs = Template->GetMacroUVRadius();
    }

    ParticleDynamicData->DynamicEmitterDataArray.Empty();
    ParticleDynamicData->DynamicEmitterDataArray.Reserve(EmitterInstances.Num());

    for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
    {
        FDynamicEmitterDataBase* NewDynamicEmitterData = nullptr;
        FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];

        if (EmitterInst)
        {
            NewDynamicEmitterData = EmitterInst->GetDynamicData(true);

            if (NewDynamicEmitterData != nullptr)
            {
                ParticleDynamicData->DynamicEmitterDataArray.Add(NewDynamicEmitterData);
                NewDynamicEmitterData->EmitterIndex = EmitterIndex;
            }
        }
    }

    return ParticleDynamicData;
}

void UParticleSystemComponent::ReBuildInstancesMemoryLayout()
{
    for (auto* Instance : EmitterInstances)
    {
        if (Instance)
        {
            Instance->AllKillParticles();
            Instance->BuildMemoryLayout();
        }
    }
}
