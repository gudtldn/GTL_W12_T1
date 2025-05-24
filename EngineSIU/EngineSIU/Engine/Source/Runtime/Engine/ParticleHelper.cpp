#include "ParticleHelper.h"

void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
    ParticleDataNumBytes = InParticleDataNumBytes;
    ParticleIndicesNumShorts = InParticleIndicesNumShorts;

    MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * sizeof(uint16);

    if (MemBlockSize > 0)
    {
        ParticleData = new uint8[MemBlockSize];

        if (ParticleIndicesNumShorts > 0)
        {
            ParticleIndices = (uint16*)(ParticleData + ParticleDataNumBytes);
        }
        else
        {
            ParticleIndices = nullptr;
        }
    }
    else
    {
        ParticleData = nullptr;
        ParticleIndices = nullptr;
    }
}

void FParticleDataContainer::Free()
{
    if (ParticleData)
    {
        free(ParticleData);
        ParticleData = nullptr;
        ParticleIndices = nullptr;
    }
    
    MemBlockSize = 0;
    ParticleDataNumBytes = 0;
    ParticleIndicesNumShorts = 0;
}

void FDynamicEmitterReplayDataBase::Serialize(FArchive& Ar)
{
    int32 EmitterTypeAsInt = static_cast<int32>(eEmitterType);
    Ar << EmitterTypeAsInt;
    eEmitterType = static_cast< EDynamicEmitterType >( EmitterTypeAsInt );

    Ar << ActiveParticleCount;
    Ar << ParticleStride;
		
    TArray<uint8> ParticleData;
    TArray<uint16> ParticleIndices;

    if (!Ar.IsLoading())
    {
        if (DataContainer.ParticleDataNumBytes)
        {
            ParticleData.AddUninitialized(DataContainer.ParticleDataNumBytes);
            memcpy(ParticleData.GetData(), DataContainer.ParticleData, DataContainer.ParticleDataNumBytes);
        }
        if (DataContainer.ParticleIndicesNumShorts)
        {
            ParticleIndices.AddUninitialized(DataContainer.ParticleIndicesNumShorts);
            memcpy(ParticleIndices.GetData(), DataContainer.ParticleIndices, DataContainer.ParticleIndicesNumShorts * sizeof(uint16));
        }
    }

    Ar << ParticleData;
    Ar << ParticleIndices;

    if (Ar.IsLoading())
    {
        DataContainer.Free();
        if (ParticleData.Num())
        {
            DataContainer.Alloc(ParticleData.Num(), ParticleIndices.Num());
            memcpy(DataContainer.ParticleData, ParticleData.GetData(), DataContainer.ParticleDataNumBytes);
            if (DataContainer.ParticleIndicesNumShorts)
            {
                memcpy(DataContainer.ParticleIndices, ParticleIndices.GetData(), DataContainer.ParticleIndicesNumShorts * sizeof(uint16));
            }
        }
    }

    Ar << Scale;
    Ar << SortMode;
    Ar << MacroUVOverride;
}
