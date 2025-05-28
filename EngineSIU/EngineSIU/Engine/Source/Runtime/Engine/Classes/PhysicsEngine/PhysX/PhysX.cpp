#include "PhysX.h"
#include "PxPhysicsAPI.h"

using namespace physx;

PxFoundation* GFoundation = nullptr;
PxPhysics* GPhysics = nullptr;
PxDefaultCpuDispatcher* GDispatcher = nullptr;
PxScene* GScene = nullptr;
PxMaterial* GMaterial = nullptr; // 기본적인 재질

#ifdef _DEBUG
namespace
{
PxPvd* Pvd = nullptr;
PxPvdTransport* PvdTransport = nullptr;
bool bPIEMode = false;
}
#endif

void FPhysX::Initialize()
{
    static PxDefaultAllocator DefaultAllocatorCallback;
    static PxDefaultErrorCallback DefaultErrorCallback;

    GFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, DefaultAllocatorCallback, DefaultErrorCallback);

#ifdef _DEBUG
    // PVD 연결
    Pvd = PxCreatePvd(*GFoundation);
    PvdTransport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    Pvd->connect(*PvdTransport, PxPvdInstrumentationFlag::eALL);
#endif

    GPhysics = PxCreatePhysics(
        PX_PHYSICS_VERSION,
        *GFoundation,
        PxTolerancesScale()
#ifdef _DEBUG
        ,
        true,
        Pvd
#endif
    );

    PxSceneDesc SceneDesc{GPhysics->getTolerancesScale()};
    SceneDesc.gravity = PxVec3{0.0f, 0.0f, -9.8f}; // 중력 설정
    GDispatcher = PxDefaultCpuDispatcherCreate(8); // 연산에 사용할 스레드 개수
    SceneDesc.cpuDispatcher = GDispatcher;
    SceneDesc.filterShader = PxDefaultSimulationFilterShader;
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
    GScene = GPhysics->createScene(SceneDesc);

#ifdef _DEBUG
    // PVD Scene 설정
    if (PxPvdSceneClient* PvdClient = GScene->getScenePvdClient())
    {
        // 물리 제약조건(예: 조인트 등)을 PVD로 전송
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);

        // 충돌 정보(예: 두 객체가 맞닿은 지점 등)를 PVD로 전송
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);

        // 씬 쿼리(예: 레이캐스트, 오버랩 등) 결과를 PVD로 전송
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }
#endif
    GMaterial = GPhysics->createMaterial(0.5f, 0.5f, 0.6f);
    // PhysX Cooking 라이브러리 초기화 (필요시)
    // PxInitExtensions(*GPhysics, pvd);
}

void FPhysX::Tick(float DeltaTime)
{
#if _DEBUG
    if (!bPIEMode)
    {
        return;
    }
#endif

    // TODO: FixedTime 변수 위치 옮기기
    static float AccumulatedTime = 0.0f;
    constexpr float FixedDeltaTime = 1.0f / 60.0f;

    AccumulatedTime += DeltaTime;

    while (AccumulatedTime >= FixedDeltaTime)
    {
        GScene->simulate(FixedDeltaTime);
        GScene->fetchResults(true);

        AccumulatedTime -= FixedDeltaTime;
    }
}

void FPhysX::Release()
{
    if (GMaterial)
    {
        GMaterial->release();
        GMaterial = nullptr;
    }

    if (GScene)
    {
        GScene->release();
        GScene = nullptr;
    }

    if (GDispatcher)
    {
        GDispatcher->release();
        GDispatcher = nullptr;
    }

    // PhysX Cooking 라이브러리 사용시
    // PxCloseExtensions();

    if (GPhysics)
    {
        GPhysics->release();
        GPhysics = nullptr;
    }

#ifdef _DEBUG
    if (Pvd)
    {
        Pvd->release();
        Pvd = nullptr;
    }

    if (PvdTransport)
    {
        PvdTransport->release();
        PvdTransport = nullptr;
    }
#endif

    if (GFoundation)
    {
        GFoundation->release();
        GFoundation = nullptr;
    }
}

void FPhysX::StartSimulatePVD()
{
#ifdef _DEBUG
    // if (PxPvdSceneClient* PvdClient = GScene->getScenePvdClient())
    // {
    //     // 물리 제약조건(예: 조인트 등)을 PVD로 전송
    //     PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
    //
    //     // 충돌 정보(예: 두 객체가 맞닿은 지점 등)를 PVD로 전송
    //     PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
    //
    //     // 씬 쿼리(예: 레이캐스트, 오버랩 등) 결과를 PVD로 전송
    //     PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    // }
    bPIEMode = true;
#endif
}

void FPhysX::EndSimulatePVD()
{
#ifdef _DEBUG
    // if (PxPvdSceneClient* PvdClient = GScene->getScenePvdClient())
    // {
    //     PvdClient->setScenePvdFlags(PxPvdSceneFlags());
    // }
    bPIEMode = false;
#endif
}
