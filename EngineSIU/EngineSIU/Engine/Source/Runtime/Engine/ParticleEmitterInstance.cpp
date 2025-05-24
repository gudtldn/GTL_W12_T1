#include "ParticleEmitterInstance.h"

#include <algorithm>
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "UObject/Casts.h"

#include "Engine/FObjLoader.h"
//#include "Particles/ParticleModuleRequired.h"

void FParticleEmitterInstance::Initialize()
{
    CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
    
    const TArray<UParticleModule*>& Modules = CurrentLODLevel->GetModules();

    if (CurrentLODLevel->RequiredModule->bUseMaxDrawCount)
    {
        MaxActiveParticles = CurrentLODLevel->CalculateMaxActiveParticleCount();
    }
    else
    {
        MaxActiveParticles = 1000;
    }

    BuildMemoryLayout();

    ParticleData = new uint8[MaxActiveParticles * ParticleStride];
    ParticleIndices = new uint16[MaxActiveParticles];
    InstanceData = new uint8[InstancePayloadSize];

    ActiveParticles = 0;
    ParticleCounter = 0;
    AccumulatedTime = 0.0f;
    SpawnFraction = 0.0f;

    bEnabled = true;
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
    AccumulatedTime += DeltaTime;
    CurrentTimeForBurst += DeltaTime;

    // 초당 생성속도에 따른 현재 프레임에 생성할 파티클 수 계산
    int32 SpawnCount = CalculateSpawnCount(DeltaTime);

    // 최대 활성 파티클 수를 초과하지 않도록 조정
    int32 AvailableParticles = MaxActiveParticles - ActiveParticles;
    SpawnCount = FMath::Min(SpawnCount, AvailableParticles);

    if (SpawnCount > 0)
    {
        float Increment = (SpawnCount > 1) ? DeltaTime / (SpawnCount - 1) : 0.0f;
        float StartTime = AccumulatedTime - DeltaTime;
        SpawnParticles(SpawnCount, StartTime, Increment, FVector::ZeroVector, FVector::ZeroVector);
    }

    UpdateModules(DeltaTime);
    UpdateParticles(DeltaTime);
}

void FParticleEmitterInstance::SpawnParticles(
    int32 Count, float StartTime, float Increment,
    const FVector& InitialLocation, const FVector& InitialVelocity)
{
    for (int32 i = 0; i < Count; i++)
    {
        int32 NextIndex = ActiveParticles++;
        DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextIndex)

        PreSpawn(Particle, InitialLocation, InitialVelocity);

        int32 Offset = 0;
        float SpawnTime = StartTime + Increment * i;

        UParticleLODLevel* LODLevel = CurrentLODLevel;
        for (auto* Module : LODLevel->GetModules())
        {
            if (Module->bSpawnModule)
            {
                Module->Spawn(this, Offset, SpawnTime, Particle);
            }
        }

        float Interp = (Count > 1) ? (float)i / (float)(Count - 1) : 0.f;
        PostSpawn(Particle, Interp, SpawnTime);
    }
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
    if (Index < 0 || Index >= ActiveParticles)
    {
        return;
    }

    int32 LastIndex = ActiveParticles - 1;
    if (Index != LastIndex)
    {
        std::memcpy(
            ParticleData + ParticleStride * Index,
            ParticleData + ParticleStride * LastIndex,
            ParticleStride
        );

        // 접근될 일은 없지만 방어적으로 기존 마지막 파티클을 초기화
        std::memset(
            ParticleData + ParticleStride * LastIndex,
            0,
            ParticleStride
        );

        // 아직은 ParticleIndices를 사용하지 않아서 의미는 없지만 추후에 사용될 수 있음
        if (ParticleIndices != nullptr)
        {
            ParticleIndices[Index] = ParticleIndices[LastIndex];
        }
    }
    ActiveParticles--;
}

int32 FParticleEmitterInstance::CalculateSpawnCount(float DeltaTime)
{
    UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;

    int32 SpawnCount = 0;

    if (SpawnModule->bProcessBurstList)
    {
        if (CurrentTimeForBurst > SpawnModule->BurstTime)
        {
            SpawnCount = SpawnModule->BurstCount;
            CurrentTimeForBurst = 0.f;
        }
    }
    else
    {
        float Rate = CurrentLODLevel->SpawnModule->Rate.GetValue();
        float RateScale = CurrentLODLevel->SpawnModule->RateScale.GetValue();

        Rate *= RateScale;

        float NewParticlesFloat = Rate * DeltaTime + SpawnFraction;
        SpawnCount = FMath::FloorToInt(NewParticlesFloat);
        SpawnFraction = NewParticlesFloat - SpawnCount;
    }

    return SpawnCount;
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
    Particle->OldLocation = InitialLocation;
    Particle->Location = InitialLocation;

    Particle->BaseVelocity = InitialVelocity;
    Particle->Velocity = InitialVelocity;

    Particle->BaseRotationRate = 0.f;
    Particle->Rotation = 0.f;
    Particle->RotationRate = 0.f;

    Particle->BaseSize = FVector(1.f, 1.f, 1.f);
    Particle->Size = Particle->BaseSize;
    Particle->Flags = 0;

    Particle->Color = Particle->BaseColor;

    Particle->BaseColor = FLinearColor::White;

    Particle->RelativeTime = 0.f;
    Particle->OneOverMaxLifetime = 0.5f; // 나중에 수명 기반 분포에서 설정 가능
    Particle->Placeholder0 = 0.f;
    Particle->Placeholder1 = 0.f;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime)
{
    Particle->OldLocation = Particle->Location;
    Particle->Location   += FVector(Particle->Velocity) * SpawnTime;
}

void FParticleEmitterInstance::UpdateModules(float DeltaTime)
{
    for (auto* Module : CurrentLODLevel->GetModules())
    {
        if (Module->bEnabled == false)
        {
            continue;
        }

        if (Module->bUpdateModule)
        {
            int32 Offset = Module->GetInstancePayloadSize();
            Module->Update(this, Offset, DeltaTime);
        }
        
        if (Module->bFinalUpdateModule)
        {
            int32 Offset = Module->GetInstancePayloadSize();
            Module->FinalUpdate(this, Offset, DeltaTime);
        }
    }
}

void FParticleEmitterInstance::AllKillParticles()
{
    for (int32 i = ActiveParticles - 1; i >= 0; i--)
    {
        KillParticle(i);
    }
    ActiveParticles = 0;
}

void FParticleEmitterInstance::BuildMemoryLayout()
{
    // BaseParticle 헤더 분량
    PayloadOffset = sizeof(FBaseParticle);
    ParticleSize = PayloadOffset;
    InstancePayloadSize = 0;

    for (auto* Module : CurrentLODLevel->GetModules())
    {
        Module->SetModulePayloadOffset(ParticleSize);
        ParticleSize += Module->GetModulePayloadSize();

        if (Module->bUpdateModule)
        {
            Module->SetInstancePayloadOffset(InstancePayloadSize);
            InstancePayloadSize += Module->GetInstancePayloadSize();
        }
    }

    ParticleStride = ParticleSize;
}

bool FParticleEmitterInstance::IsDynamicDataRequired()
{
    if (ActiveParticles <= 0 || !SpriteTemplate)
    {
        return false;
    }

    if (CurrentLODLevel == nullptr || CurrentLODLevel->bEnabled == false ||
        ((CurrentLODLevel->RequiredModule->bUseMaxDrawCount == true) && (CurrentLODLevel->RequiredModule->MaxDrawCount == 0)))
    {
        return false;
    }

    if (Component == nullptr)
    {
        return false;
    }
    return true;
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
    for (int32 i = 0; i < ActiveParticles; i++)
    {
        DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * i)

        // 상대 시간 업데이트
        Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;

        // 수명 종료 판단
        if (Particle->RelativeTime >= 1.0f)
        {
            KillParticle(i);
            i--; // 마지막 인덱스를 앞으로 복사했으므로 인덱스 유지
            continue;
        }

        // 위치 업데이트
        Particle->OldLocation = Particle->Location;
        Particle->Velocity = Particle->BaseVelocity;
        Particle->Location += Particle->Velocity * DeltaTime;

        // 회전 업데이트
        Particle->RotationRate = Particle->BaseRotationRate;
        Particle->Rotation += Particle->RotationRate * DeltaTime;

        // 크기 업데이트
        Particle->Size = Particle->BaseSize;
    }
}

bool FParticleEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
    // Make sure there is a template present
    if (!SpriteTemplate)
    {
        return false;
    }

    // Allocate it for now, but we will want to change this to do some form
    // of caching
    if (ActiveParticles <= 0 || !bEnabled)
    {
        return false;
    }
    // If the template is disabled, don't return data.
    if ((CurrentLODLevel == nullptr) || (CurrentLODLevel->bEnabled == false))
    {
        return false;
    }

    OutData.eEmitterType = EDynamicEmitterType::DET_Unknown;

    OutData.ActiveParticleCount = ActiveParticles;
    OutData.ParticleStride = ParticleStride;
    // OutData.SortMode = SortMode;

    OutData.Scale = FVector::OneVector;
    if (Component)
    {
        OutData.Scale = FVector(Component->GetComponentTransform().GetScale3D());
    }

    int32 ParticleMemSize = MaxActiveParticles * ParticleStride;
    OutData.DataContainer.Alloc(ParticleMemSize, MaxActiveParticles);

    memcpy(OutData.DataContainer.ParticleData, ParticleData, ParticleMemSize);
    memcpy(OutData.DataContainer.ParticleIndices, ParticleIndices, MaxActiveParticles * sizeof(uint16));

    FDynamicSpriteEmitterReplayData* SpriteReplayData = dynamic_cast<FDynamicSpriteEmitterReplayData*>(&OutData);
    if (SpriteReplayData && SpriteTemplate->EmitterType == EDynamicEmitterType::DET_Sprite)
    {
        SpriteReplayData->SubImages_Horizontal = CurrentLODLevel->RequiredModule->SubImagesHorizontal;
        SpriteReplayData->SubImages_Vertical = CurrentLODLevel->RequiredModule->SubImagesVertical;
        SpriteReplayData->SubUVDataOffset = SubUVDataOffset;
    }

    return true;
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module)
{
    if (SpriteTemplate == nullptr)
    {
        return 0;
    }
    
    uint32* Offset = SpriteTemplate->ModuleOffsetMap.Find(Module);
    return (Offset != nullptr) ? *Offset : 0;
}


//////////////////////// SpriteEmitter
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData(bool bSelected)
{
    // It is valid for the LOD level to be NULL here!
    if (IsDynamicDataRequired() == false || !bEnabled)
    {
        return nullptr;
    }

    // Allocate the dynamic data
    FDynamicSpriteEmitterData* NewEmitterData = new FDynamicSpriteEmitterData(CurrentLODLevel->RequiredModule);

    // Now fill in the source data
    if(!FillReplayData( NewEmitterData->Source ) )
    {
        delete NewEmitterData;
        return nullptr;
    }

    // Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
    NewEmitterData->Init( bSelected );

    return NewEmitterData;
}

bool FParticleSpriteEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData) // TODO: 필요한 거 주석 해제
{
    if (ActiveParticles <= 0)
    {
        return false;
    }

    // Call parent implementation first to fill in common particle source data
    if(!FParticleEmitterInstance::FillReplayData( OutData ) )
    {
        return false;
    }

    OutData.eEmitterType = EDynamicEmitterType::DET_Sprite;
    SpriteTemplate->EmitterType = EDynamicEmitterType::DET_Sprite;

    FDynamicSpriteEmitterReplayDataBase* NewReplayData = dynamic_cast< FDynamicSpriteEmitterReplayDataBase* >( &OutData );

    NewReplayData->MaterialInterface = CurrentLODLevel->RequiredModule->MaterialInterface;
    NewReplayData->RequiredModule = CurrentLODLevel->RequiredModule->CreateRendererResource();
    NewReplayData->MaterialInterface = CurrentLODLevel->RequiredModule->MaterialInterface;
    
    // NewReplayData->InvDeltaSeconds = (LastDeltaTime > KINDA_SMALL_NUMBER) ? (1.0f / LastDeltaTime) : 0.0f;
    // NewReplayData->LWCTile = ((Component == nullptr) || CurrentLODLevel->RequiredModule->bUseLocalSpace) ? FVector::Zero() : Component->GetLWCTile();

    NewReplayData->MaxDrawCount = CurrentLODLevel->RequiredModule->bUseMaxDrawCount == true ? CurrentLODLevel->RequiredModule->MaxDrawCount : -1;
    // NewReplayData->ScreenAlignment	= CurrentLODLevel->RequiredModule->ScreenAlignment;
    // NewReplayData->bUseLocalSpace = CurrentLODLevel->RequiredModule->bUseLocalSpace;
    // NewReplayData->EmitterRenderMode = SpriteTemplate->EmitterRenderMode;
    // NewReplayData->DynamicParameterDataOffset = DynamicParameterDataOffset;
    // NewReplayData->LightDataOffset = LightDataOffset;
    // NewReplayData->LightVolumetricScatteringIntensity = LightVolumetricScatteringIntensity;
    // NewReplayData->CameraPayloadOffset = CameraPayloadOffset;

    // NewReplayData->SubUVDataOffset = SubUVDataOffset;
    // NewReplayData->SubImages_Horizontal = CurrentLODLevel->RequiredModule->SubImages_Horizontal;
    // NewReplayData->SubImages_Vertical = CurrentLODLevel->RequiredModule->SubImages_Vertical;

    // NewReplayData->MacroUVOverride.bOverride = CurrentLODLevel->RequiredModule->bOverrideSystemMacroUV;
    // NewReplayData->MacroUVOverride.Radius = CurrentLODLevel->RequiredModule->MacroUVRadius;
    // NewReplayData->MacroUVOverride.Position = FVector(CurrentLODLevel->RequiredModule->MacroUVPosition);
        
    // NewReplayData->bLockAxis = false;
    // if (bAxisLockEnabled == true)
    // {
        // NewReplayData->LockAxisFlag = LockAxisFlags;
        // if (LockAxisFlags != EPAL_NONE)
        // {
            // NewReplayData->bLockAxis = true;
        // }
    // }

    // // If there are orbit modules, add the orbit module data
    // if (LODLevel->OrbitModules.Num() > 0)
    // {
    // 	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
    // 	UParticleModuleOrbit* LastOrbit = HighestLODLevel->OrbitModules[LODLevel->OrbitModules.Num() - 1];
    // 	check(LastOrbit);
    //
    // 	uint32* LastOrbitOffset = SpriteTemplate->ModuleOffsetMap.Find(LastOrbit);
    // 	NewReplayData->OrbitModuleOffset = *LastOrbitOffset;
    // }

    // NewReplayData->EmitterNormalsMode = CurrentLODLevel->RequiredModule->EmitterNormalsMode;
    // NewReplayData->NormalsSphereCenter = (FVector)CurrentLODLevel->RequiredModule->NormalsSphereCenter;
    // NewReplayData->NormalsCylinderDirection = (FVector)CurrentLODLevel->RequiredModule->NormalsCylinderDirection;

    // NewReplayData->PivotOffset = FVector2D(PivotOffset);

    // NewReplayData->bUseVelocityForMotionBlur = CurrentLODLevel->RequiredModule->ShouldUseVelocityForMotionBlur();
    // NewReplayData->bRemoveHMDRoll = CurrentLODLevel->RequiredModule->bRemoveHMDRoll;
    // NewReplayData->MinFacingCameraBlendDistance = CurrentLODLevel->RequiredModule->MinFacingCameraBlendDistance;
    // NewReplayData->MaxFacingCameraBlendDistance = CurrentLODLevel->RequiredModule->MaxFacingCameraBlendDistance;

    return true;
}

FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData(bool bSelected)
{
    // It is valid for the LOD level to be NULL here!
    if (IsDynamicDataRequired() == false || !bEnabled)
    {
        return nullptr;
    }

    // Allocate the dynamic data
    FDynamicMeshEmitterData* NewEmitterData = new FDynamicMeshEmitterData(CurrentLODLevel->RequiredModule);

    // Now fill in the source data
    if (!FillReplayData(NewEmitterData->Source))
    {
        delete NewEmitterData;
        return nullptr;
    }

    // Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
    NewEmitterData->StaticMesh = FObjManager::CreateStaticMesh("Contents/Cube/cube-tex.obj");

    return NewEmitterData;
}

bool FParticleMeshEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
    if (ActiveParticles <= 0)
    {
        return false;
    }

    // Call parent implementation first to fill in common particle source data
    if (!FParticleEmitterInstance::FillReplayData(OutData))
    {
        return false;
    }

    OutData.eEmitterType = EDynamicEmitterType::DET_Mesh;
    SpriteTemplate->EmitterType = EDynamicEmitterType::DET_Mesh;

    FDynamicMeshEmitterReplayData* NewReplayData = dynamic_cast<FDynamicMeshEmitterReplayData*>(&OutData);

    NewReplayData->MaterialInterface = CurrentLODLevel->RequiredModule->MaterialInterface;
    NewReplayData->RequiredModule = CurrentLODLevel->RequiredModule->CreateRendererResource();
    NewReplayData->MaterialInterface = CurrentLODLevel->RequiredModule->MaterialInterface;

    // NewReplayData->InvDeltaSeconds = (LastDeltaTime > KINDA_SMALL_NUMBER) ? (1.0f / LastDeltaTime) : 0.0f;
    // NewReplayData->LWCTile = ((Component == nullptr) || CurrentLODLevel->RequiredModule->bUseLocalSpace) ? FVector::Zero() : Component->GetLWCTile();

    NewReplayData->MaxDrawCount = CurrentLODLevel->RequiredModule->bUseMaxDrawCount == true ? CurrentLODLevel->RequiredModule->MaxDrawCount : -1;

    return true;
}
