#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "ParticleHelper.h"

class UParticleModule;
class UParticleLODLevel;

enum class EParticleBurstMethod : uint8
{
    EPBM_Instant,
    EPBM_Interpolated,
    EPBM_MAX,
};

enum EParticleSubUVInterpMethod : int
{
    PSUVIM_None,
    PSUVIM_Linear,
    PSUVIM_Linear_Blend,
    PSUVIM_Random,
    PSUVIM_Random_Blend,
    PSUVIM_MAX,
};

class UParticleEmitter : public UObject
{
    DECLARE_CLASS(UParticleEmitter, UObject)

public:
    UParticleEmitter();
    virtual ~UParticleEmitter() override = default;

    void CacheEmitterModuleInfo();

    virtual void DisplayProperty() override;

    TArray<UParticleLODLevel*> GetLODLevels() const { return LODLevels; }
    TMap<UParticleModule*, uint32> ModuleOffsetMap;
    UParticleLODLevel* GetLODLevel(int32 LODIndex) const;

public:
    UPROPERTY_WITH_FLAGS(EditAnywhere, FName, EmitterName, = "Default")
    int32 PeakActiveParticles = 0;

    // Below is information udpated by calling CacheEmitterModuleInfo
    
    int32 ParticleSize;

    EDynamicEmitterType EmitterType;

private:
    TArray<UParticleLODLevel*> LODLevels; // 현재는 Level 0만 사용
};
