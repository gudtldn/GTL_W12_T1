#pragma once


struct FSimulationEventCallback;

namespace physx
{
class PxPvdTransport;
class PxPvd;
class PxMaterial;
class PxScene;
class PxDefaultCpuDispatcher;
class PxPhysics;
class PxFoundation;
}

struct FPhysX
{
    static void Initialize();
    static void Tick(float DeltaTime);
    static void Release();

    static void StartSimulatePVD();
    static void EndSimulatePVD();

public:
    inline static physx::PxFoundation* GFoundation = nullptr;
    inline static physx::PxPhysics* GPhysics = nullptr;
    inline static physx::PxDefaultCpuDispatcher* GDispatcher = nullptr;
    inline static physx::PxScene* GScene = nullptr;
    inline static physx::PxMaterial* GMaterial = nullptr; // 기본적인 재질

    inline static FSimulationEventCallback* GSimulationEventCallback = nullptr;

#ifdef _DEBUG
    inline static physx::PxPvd* Pvd = nullptr;
    inline static physx::PxPvdTransport* PvdTransport = nullptr;
    inline static bool bPIEMode = false;
#endif
};
