#include "PhysicsEngine.h"

physx::PxFoundation* gFoundation = nullptr;
physx::PxPhysics* gPhysics = nullptr;
physx::PxScene* gScene = nullptr;
physx::PxMaterial* gMaterial = nullptr;
physx::PxDefaultCpuDispatcher* gDispatcher = nullptr;


TArray<FGameObject> gObjects;

#ifdef _DEBUG
namespace
{
    PxPvd* Pvd = nullptr;
    PxPvdTransport* PvdTransport = nullptr;
    bool bPIEMode = false;
}
#endif

void FPhysicsEngine::InitPhysX()
{
    static PxDefaultAllocator DefaultAllocatorCallback;
    static PxDefaultErrorCallback DefaultErrorCallback;

    gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, DefaultAllocatorCallback, DefaultErrorCallback);

#ifdef _DEBUG
    // PVD 연결
    Pvd = PxCreatePvd(*gFoundation);
    PvdTransport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    Pvd->connect(*PvdTransport, PxPvdInstrumentationFlag::eALL);
#endif

    gPhysics = PxCreatePhysics(
        PX_PHYSICS_VERSION,
        *gFoundation,
        PxTolerancesScale()
#ifdef _DEBUG
        ,
        true,
        Pvd
#endif
    );

    gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.6f);
    physx::PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0, 0, -9.81f);
    gDispatcher = physx::PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = gDispatcher;
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    gScene = gPhysics->createScene(sceneDesc);

#ifdef _DEBUG
    // PVD Scene 설정
    if (PxPvdSceneClient* PvdClient = gScene->getScenePvdClient())
    {
        // 물리 제약조건(예: 조인트 등)을 PVD로 전송
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);

        // 충돌 정보(예: 두 객체가 맞닿은 지점 등)를 PVD로 전송
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);

        // 씬 쿼리(예: 레이캐스트, 오버랩 등) 결과를 PVD로 전송
        PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }
#endif
}

void FPhysicsEngine::ShutdownPhysX()
{
    if (gScene)
    {
        gScene->release();
        gScene = nullptr;
    }
    if (gDispatcher)
    {
        gDispatcher->release();
        gDispatcher = nullptr;
    }
    if (gMaterial)
    {
        gMaterial->release();
        gMaterial = nullptr;
    }
    if (gPhysics)
    {
        gPhysics->release();
        gPhysics = nullptr;
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

    if (gFoundation)
    {
        gFoundation->release();
        gFoundation = nullptr;
    }
}
void FPhysicsEngine::Tick(float DeltaTime)
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
        FPhysicsEngine::Simulate(FixedDeltaTime);
        AccumulatedTime -= FixedDeltaTime;
    }
}

void FPhysicsEngine::StartSimulatePVD()
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

void FPhysicsEngine::EndSimulatePVD()
{
#ifdef _DEBUG
    // if (PxPvdSceneClient* PvdClient = GScene->getScenePvdClient())
    // {
    //     PvdClient->setScenePvdFlags(PxPvdSceneFlags());
    // }
    bPIEMode = false;
#endif
}


void FPhysicsEngine::Simulate(float DeltaTime)
{
    gScene->simulate(DeltaTime);
    FetchResults(true);
    ProcessDeferredPhysicsOperations();
}

bool FPhysicsEngine::FetchResults(bool Block)
{
    return gScene->fetchResults(Block);
}

FPhysicsAggregateHandle FPhysicsEngine::CreateAggregate(uint32 MaxActors, bool EnableSelfCollision)
{
    physx::PxPhysics* PxPhysicsSDK = gPhysics;
    if (gScene && PxPhysicsSDK)
    {
        physx::PxAggregate* NewPxAgg = PxPhysicsSDK->createAggregate(MaxActors, EnableSelfCollision);
        if (NewPxAgg)
        {
            gScene->addAggregate(*NewPxAgg);
            return FPhysicsAggregateHandle(NewPxAgg);
        }
    }
    return FPhysicsAggregateHandle();
}

void FPhysicsEngine::ReleaseAggregate(FPhysicsAggregateHandle AggregateHandle)
{
    if (gScene && AggregateHandle.IsValid())
    {
        physx::PxAggregate* PxAgg = AggregateHandle.GetUnderlyingAggregate();
        if (PxAgg)
        {
            gScene->removeAggregate(*PxAgg);
            PxAgg->release();
        }
    }
}

void FPhysicsEngine::DeferPhysicsStateCreation(UPrimitiveComponent* Primitive)
{
    if (Primitive && !DeferredCreationQueue.Contains(Primitive))
    {
        DeferredCreationQueue.Add(Primitive);
    }
}

void FPhysicsEngine::RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Primitive)
{
    if (Primitive)
    {
        DeferredCreationQueue.Remove(Primitive);
    }
}

void FPhysicsEngine::ProcessDeferredPhysicsOperations()
{
    for (auto& obj : gObjects)
    {
        obj.UpdateFromPhysics();
    }
}
