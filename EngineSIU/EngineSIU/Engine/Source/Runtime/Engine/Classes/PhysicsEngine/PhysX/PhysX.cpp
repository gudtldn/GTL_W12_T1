#include "PhysX.h"

#include "FSimulationEventCallback.h"
#include "PxPhysicsAPI.h"

using namespace physx;


static PxFilterFlags ContactReportFilterShader(
    PxFilterObjectAttributes Attributes0, PxFilterData FilterData0,
    PxFilterObjectAttributes Attributes1, PxFilterData FilterData1,
    PxPairFlags& PairFlags, const void* ConstantBlock, PxU32 ConstantBlockSize
)
{
    PX_UNUSED(ConstantBlockSize);
    PX_UNUSED(ConstantBlock);

    // 1. 블로킹 충돌 여부 결정
    //    (obj0의 타입이 obj1의 블로킹 마스크에 포함되고, obj1의 타입이 obj0의 블로킹 마스크에 포함되는지)
    bool bBlock = ((FilterData0.word0 & FilterData1.word1) != 0) && ((FilterData1.word0 & FilterData0.word1) != 0);

    if (bBlock)
    {
        PairFlags = PxPairFlag::eCONTACT_DEFAULT; // 물리적 반작용

        // 2. onContact 이벤트 발생 여부 결정 (word2를 터치 마스크로 사용한다고 가정)
        bool bNotifyTouch = ((FilterData0.word0 & FilterData1.word2) != 0) && ((FilterData1.word0 & FilterData0.word2) != 0);
        if (bNotifyTouch)
        {
            PairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
            PairFlags |= PxPairFlag::eNOTIFY_CONTACT_POINTS; // 필요시 추가
            // PairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;     // 필요시 추가
        }
        return PxFilterFlag::eDEFAULT;
    }

    // TODO: 오버랩(트리거) 이벤트 처리 로직 (word2 또는 다른 word를 오버랩 마스크로 사용)
    // bool bOverlap = (filterData0.word0 & filterData1.word_for_overlap_mask) && ...
    // if (bOverlap) {
    //    pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT; // 물리 반응은 없고 이벤트만
    //    return physx::PxFilterFlag::eDEFAULT;
    // }


    return PxFilterFlag::eSUPPRESS; // 그 외에는 충돌 및 이벤트 없음

    // all initial and persisting reports for everything, with per-point data
    // PairFlags = PxPairFlag::eSOLVE_CONTACT     // 물리적 충돌 해결 (반작용)
    //     | PxPairFlag::eDETECT_DISCRETE_CONTACT // 이산 충돌 감지
    //     | PxPairFlag::eNOTIFY_TOUCH_FOUND      // 접촉 시작 시 알림
    //     // | PxPairFlag::eNOTIFY_TOUCH_PERSISTS   // 접촉 유지 시 알림
    //     | PxPairFlag::eNOTIFY_TOUCH_LOST      // 접촉 종료 시 알림
    //     | PxPairFlag::eNOTIFY_CONTACT_POINTS; // 접촉점 정보 요청
    // return PxFilterFlag::eDEFAULT;

    // TODO: BodySetup에서 ECollisionChannel 만들어서 word 설정하기
    // #define HIT_EVENT_FLAG (1 << 0) // word2의 첫 번째 비트
    //
    //     // 1. 기본적인 충돌 여부 결정 (서로 마스크에 포함되는지)
    //     if ((FilterData0.word0 & FilterData1.word1) && (FilterData1.word0 & FilterData0.word1))
    //     {
    //         // 충돌 발생! 물리적 반응 설정
    //         PairFlags = PxPairFlag::eCONTACT_DEFAULT;
    //
    //         // 2. 추가적으로 이벤트 알림이 필요한 경우에만 알림 플래그 설정
    //         const bool bGenerateHitEvents0 = (FilterData0.word2 & HIT_EVENT_FLAG) != 0;
    //         const bool bGenerateHitEvents1 = (FilterData1.word2 & HIT_EVENT_FLAG) != 0;
    //
    //         if (bGenerateHitEvents0 || bGenerateHitEvents1) // 둘 중 하나라도 히트 이벤트를 원하면
    //         {
    //             PairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
    //             PairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;
    //             PairFlags |= PxPairFlag::eNOTIFY_CONTACT_POINTS;
    //
    //             // TOUCH_LOST, TOUCH_PERSISTS 등도 특정 조건에 따라 추가 가능
    //         }
    //
    //         // CCD 처리
    //         if (PxFilterObjectIsKinematic(Attributes0) || PxFilterObjectIsKinematic(Attributes1))
    //         {
    //             PairFlags |= PxPairFlag::eDETECT_CCD_CONTACT;
    //         }
    //
    //         return PxFilterFlag::eDEFAULT;
    //     }
    //
    //     // 그 외의 경우는 충돌하지 않음 (물리 반응도, 이벤트도 없음)
    //     return PxFilterFlag::eSUPPRESS;
    //
    // #undef HIT_EVENT_FLAG
}

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

    GSimulationEventCallback = new FSimulationEventCallback{};

    PxSceneDesc SceneDesc{GPhysics->getTolerancesScale()};
    SceneDesc.gravity = PxVec3{0.0f, 0.0f, -9.8f}; // 중력 설정
    GDispatcher = PxDefaultCpuDispatcherCreate(8); // 연산에 사용할 스레드 개수
    SceneDesc.cpuDispatcher = GDispatcher;
    SceneDesc.filterShader = ContactReportFilterShader;
    SceneDesc.simulationEventCallback = GSimulationEventCallback;
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
    SceneDesc.ccdMaxPasses = 4;
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
    PxInitExtensions(
        *GPhysics,
#ifdef _DEBUG
        Pvd
#else
        nullptr
#endif
    );
}

void FPhysX::Tick(float DeltaTime)
{
#ifdef _DEBUG
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
        GScene->setSimulationEventCallback(nullptr);
        GScene->release();
        GScene = nullptr;
    }

    if (GSimulationEventCallback)
    {
        delete GSimulationEventCallback;
        GSimulationEventCallback = nullptr;
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
