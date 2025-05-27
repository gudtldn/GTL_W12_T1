#include "CarActor.h"
#include "Components/CubeComp.h"
#include "Components/SphereComp.h"
#include "Engine/FObjLoader.h"
#include "foundation/PxMath.h"

using namespace physx;

physx::PxVehicleDrivableSurfaceToTireFrictionPairs* ACarActor::GTireFrictionPairs = nullptr;
physx::PxBatchQuery* ACarActor::GSuspensionRaycastBatchQuery = nullptr;
physx::PxQueryFilterData ACarActor::GRaycastQueryFilterData;

extern PxScene* GScene;
extern PxPhysics* GPhysics;
extern PxCooking* GCooking;
extern PxMaterial* GMaterial;

ACarActor::ACarActor()
    : PhysXChassisActor(nullptr), PhysXVehicleDrive4W(nullptr)
    , PhysXWheelsSimData(nullptr), PhysXDriveSimData4W(nullptr), bIsInScene(false)
    , bIsPhysXInitialized(false)
{
    AABBMin = FVector(-5.f, -3.f, -2.f);
    AABBMax = FVector(5.f, 3.f, 2.f);

    FObjManager::CreateStaticMesh(TEXT("Contents/Primitives/CubePrimitive.obj"));
    FObjManager::CreateStaticMesh(TEXT("Contents/Primitives/SpherePrimitive.obj"));

    Body = AddComponent<UCubeComp>(TEXT("BodyCube"));
    SetRootComponent(Body);
    Body->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Primitives/CubePrimitive.obj"));

    Body->AABB.MinLocation = AABBMin;
    Body->AABB.MaxLocation = AABBMax;
    Body->SetWorldScale3D(AABBMax - AABBMin);

    WheelRadiusVisual = VehicleParams::WheelRadius;

    FVector WheelOffsets[4] = {
        FVector(AABBMax.X, AABBMax.Y, AABBMin.Z - WheelRadiusVisual), // 앞 오른쪽
        FVector(AABBMax.X, AABBMin.Y, AABBMin.Z - WheelRadiusVisual), // 앞 왼쪽
        FVector(AABBMin.X, AABBMax.Y, AABBMin.Z - WheelRadiusVisual), // 뒤 오른쪽
        FVector(AABBMin.X, AABBMin.Y, AABBMin.Z - WheelRadiusVisual)  // 뒤 왼쪽
    };


    for (int i = 0; i < 4; ++i)
    {
        FString WheelName = FString::Printf(TEXT("Wheel0%d"), i);
        Wheels[i] = AddComponent<USphereComp>(*WheelName);
        Wheels[i]->SetWorldScale3D(FVector(WheelRadiusVisual));
        Wheels[i]->SetupAttachment(Body);
        Wheels[i]->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Primitives/SpherePrimitive.obj"));

        // 바퀴 위치를 AABB 기준 네 귀퉁이로 설정
        Wheels[i]->SetRelativeLocation(WheelOffsets[i]);
    }
    VehicleWheelQueryStateForUpdates[0].wheelQueryResults = InternalWheelQueryResults;
    VehicleWheelQueryStateForUpdates[0].nbWheelQueryResults = 4;
}

ACarActor::~ACarActor()
{
    if (PhysXVehicleDrive4W)
    {
        PhysXVehicleDrive4W->free(); // PxVehicleDrive4W::free() 호출
        PhysXVehicleDrive4W = nullptr;
    }
    if (PhysXWheelsSimData)
    {
        PhysXWheelsSimData->free();
        PhysXWheelsSimData = nullptr;
    }
    if (PhysXDriveSimData4W)
    {
        delete PhysXDriveSimData4W;
        PhysXDriveSimData4W = nullptr;
    }

    // PhysXChassisActor는 RemoveFromScene에서 씬에서 제거되고,
    // 씬이 해제될 때 또는 명시적으로 PxScene::releaseActor() 등으로 해제됨.
    // PxRigidDynamic::release()는 액터가 씬에 속해있지 않을 때 호출해야 함.
    // 여기서는 포인터만 null로. 실제 release는 씬 정리 시점에.
    if (PhysXChassisActor) {
        PhysXChassisActor = nullptr;
    }
}

bool ACarActor::InitializeVehicleSDKGlobal(physx::PxPhysics& PhysXSDK)
{
    if (!PxInitVehicleSDK(PhysXSDK))
    {
        // LogError(TEXT("PxInitVehicleSDK failed!")); // 엔진의 로깅 함수
        return false;
    }
    // 엔진 좌표계: Z-Up, X-Forward, Y-Right (왼손 좌표계)
    // PhysX 차량 내부 좌표계 설정: Up = Engine Z, Forward = Engine X
    // 결과: PhysX 차량 로컬 X = Forward, Z = Up, Y = Left (PhysX 왼손 좌표계 유지)
    PxVehicleSetBasisVectors(physx::PxVec3(0, 0, 1), physx::PxVec3(1, 0, 0));
    return true;
}

void ACarActor::ShutdownVehicleSDKGlobal()
{
    PxCloseVehicleSDK();
}

bool ACarActor::SetupCommonVehicleDataGlobal(physx::PxPhysics& PhysXSDK, physx::PxScene& PhysXScene, physx::PxMaterial& DefaultGroundMaterial)
{
    // 1. 타이어 마찰력 데이터 설정
    PxVehicleDrivableSurfaceType SurfaceTypes[1];
    SurfaceTypes[0].mType = 0; // 기본 지면 타입 ID
    const physx::PxMaterial* DrivableMaterials[1] = { &DefaultGroundMaterial };

    GTireFrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1); // (타이어 타입 수, 지면 타입 수)
    GTireFrictionPairs->setup(1, 1, DrivableMaterials, SurfaceTypes);
    GTireFrictionPairs->setTypePairFriction(0, 0, VehicleParams::TireFrictionMultiplier); // (타이어타입0, 지면타입0, 마찰계수)

    // 2. 서스펜션 레이캐스트 배치 쿼리 설정
    physx::PxU32 MaxNumWheelsInScene = 1 * 4; // 예시: 1대 차량, 휠 4개
    physx::PxBatchQueryDesc BatchQueryDesc(MaxNumWheelsInScene, 0, 0);

    // 레이캐스트 필터링: 모든 정적(static) 지오메트리와 충돌 (간단한 예시)
    // 실제로는 PxQueryFilterCallback 등으로 더 정교한 필터링 필요 (차량 자신 제외 등)
    GRaycastQueryFilterData = physx::PxQueryFilterData(physx::PxFilterData(), physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER);
    // BatchQueryDesc.preFilterShader = YourCustomFilterShader; // 고급 필터링

    GSuspensionRaycastBatchQuery = PhysXScene.createBatchQuery(BatchQueryDesc);
    if (!GSuspensionRaycastBatchQuery)
    {
        // LogError(TEXT("Failed to create PxBatchQuery for suspension raycasts."));
        GTireFrictionPairs->release();
        GTireFrictionPairs = nullptr;
        return false;
    }
    return true;
}

void ACarActor::ReleaseCommonVehicleDataGlobal()
{
    if (GTireFrictionPairs)
    {
        GTireFrictionPairs->release();
        GTireFrictionPairs = nullptr;
    }
    if (GSuspensionRaycastBatchQuery)
    {
        GSuspensionRaycastBatchQuery->release();
        GSuspensionRaycastBatchQuery = nullptr;
    }
}

void ACarActor::InitializePhysics(physx::PxPhysics* InPhysXSDK, physx::PxCooking* InPhysXCooking, physx::PxMaterial* InDefaultChassisMaterial, const FVector& InitialPosition_Engine, const FQuat& InitialRotation_Engine)
{
    // 1. 섀시 PhysX 액터 생성 (엔진 좌표계 값으로 PhysX 액터의 글로벌 포즈 설정)
    physx::PxTransform InitialGlobalPose_PhysX(
        physx::PxVec3(InitialPosition_Engine.X, InitialPosition_Engine.Y, InitialPosition_Engine.Z),
        physx::PxQuat(InitialRotation_Engine.X, InitialRotation_Engine.Y, InitialRotation_Engine.Z, InitialRotation_Engine.W)
    );
    CreateChassisPhysXActor_Internal(InPhysXSDK, InPhysXCooking, InDefaultChassisMaterial, InitialGlobalPose_PhysX);

    // 2. 휠 중심 오프셋 계산 (PhysX 차량 로컬 좌표계: X-Fwd, Z-Up, Y-Left)
    FVector ChassisHalfSizeVisual_Engine = (AABBMax - AABBMin) * 0.5f; // 엔진 좌표계 (X:Fwd, Y:Right, Z:Up)
    physx::PxVec3 WheelCenterOffsets_PhysXVehicleLocal[4];

    // PhysX 차량 로컬 Z (상방) 오프셋: 섀시 CoM 기준 휠 중심의 Z 위치. 보통 섀시 CoM보다 낮으므로 음수.
    const float WheelZOffset_PhysXVehicleLocal = -ChassisHalfSizeVisual_Engine.Z + VehicleParams::WheelRadius * 0.75f; // (예시 값)

    // PhysX 차량 로컬 (X:Fwd, Y:Left, Z:Up)
    // 엔진 Y(Right) -> PhysX Y(Left)는 부호 반전.
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_RIGHT] = physx::PxVec3(ChassisHalfSizeVisual_Engine.X * 0.8f, -ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_LEFT] = physx::PxVec3(ChassisHalfSizeVisual_Engine.X * 0.8f, ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_RIGHT] = physx::PxVec3(-ChassisHalfSizeVisual_Engine.X * 0.8f, -ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_LEFT] = physx::PxVec3(-ChassisHalfSizeVisual_Engine.X * 0.8f, ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);

    CreateVehiclePhysXSimulationData_Internal(WheelCenterOffsets_PhysXVehicleLocal);

    // 3. PxVehicleDrive4W 객체 생성 및 설정
    PhysXVehicleDrive4W = PxVehicleDrive4W::allocate(4); // 4개 휠 모두 구동
    PhysXVehicleDrive4W->setup(InPhysXSDK, PhysXChassisActor, *PhysXWheelsSimData, *PhysXDriveSimData4W, 0); // 마지막 인자: 비구동휠 수

    // 4. 초기 상태 설정
    PhysXVehicleDrive4W->setToRestState();
    PhysXVehicleDrive4W->mDriveDynData.setUseAutoGears(true);
}

void ACarActor::CreateChassisPhysXActor_Internal(physx::PxPhysics* InPhysXSDK, physx::PxCooking* InPhysXCooking, physx::PxMaterial* InDefaultChassisMaterial, const physx::PxTransform& GlobalPose_Engine)
{
    // 시각적 AABB (엔진: X-Fwd, Y-Right, Z-Up)의 반쪽 크기
    FVector ChassisHalfExtents_Engine = (AABBMax - AABBMin) * 0.5f;

    // PhysX 섀시 Box 지오메트리 (PhysX 차량 로컬: X-Fwd, Y-Left, Z-Up)
    // 엔진 X(길이) -> PhysX 로컬 X (길이)
    // 엔진 Y(폭)   -> PhysX 로컬 Y (폭)
    // 엔진 Z(높이) -> PhysX 로컬 Z (높이)
    physx::PxBoxGeometry ChassisGeom_PhysXVehicleLocal(ChassisHalfExtents_Engine.X, ChassisHalfExtents_Engine.Y, ChassisHalfExtents_Engine.Z);

    PhysXChassisActor = InPhysXSDK->createRigidDynamic(GlobalPose_Engine); // PhysX 액터의 글로벌 포즈는 엔진 월드 좌표계
    physx::PxShape* ChassisShape = PxRigidActorExt::createExclusiveShape(*PhysXChassisActor, ChassisGeom_PhysXVehicleLocal, *InDefaultChassisMaterial);

    // 섀시 필터 데이터 설정 (예: 레이캐스트 시 자신을 무시)
    // PxFilterData ChassisQueryFilterData(...); ChassisShape->setQueryFilterData(ChassisQueryFilterData);
    // PxFilterData ChassisSimFilterData(...);   ChassisShape->setSimulationFilterData(ChassisSimFilterData);

    PxRigidBodyExt::updateMassAndInertia(*PhysXChassisActor, VehicleParams::ChassisMass);
}
 
void ACarActor::CreateVehiclePhysXSimulationData_Internal(const physx::PxVec3 WheelCenterOffsets_PhysXVehicleLocal[4])
{
    // 1. 휠, 타이어, 서스펜션 데이터 (PxVehicleWheelsSimData)
    PhysXWheelsSimData = PxVehicleWheelsSimData::allocate(4);
    physx::PxVehicleWheelData WheelData[4];
    physx::PxVehicleTireData TireData[4];
    physx::PxVehicleSuspensionData SuspData[4];

    // 서스펜션 이동 방향 (PhysX 차량 로컬 좌표계: X-Fwd, Z-Up, Y-Left 이므로, Z축의 반대방향)
    const physx::PxVec3 SuspTravelDir_PhysXVehicleLocal(0.0f, 0.0f, -1.0f);

    for (int i = 0; i < 4; ++i)
    {
        WheelData[i].mRadius = VehicleParams::WheelRadius;
        WheelData[i].mMass = VehicleParams::WheelMass;
        WheelData[i].mMOI = 0.5f * VehicleParams::WheelMass * (VehicleParams::WheelRadius * VehicleParams::WheelRadius);
        WheelData[i].mWidth = VehicleParams::WheelWidth;
        WheelData[i].mDampingRate = VehicleParams::WheelDampingRate;
        WheelData[i].mMaxSteer = (i == physx::PxVehicleDrive4WWheelOrder::eFRONT_LEFT || i == physx::PxVehicleDrive4WWheelOrder::eFRONT_RIGHT) ? VehicleParams::MaxSteerAngleRad : 0.0f;
        WheelData[i].mToeAngle = 0.0f;

        TireData[i].mType = 0;
        TireData[i].mLatStiffX = VehicleParams::TireLatStiffX;
        TireData[i].mLatStiffY = VehicleParams::TireLatStiffY;
        TireData[i].mLongitudinalStiffnessPerUnitGravity = VehicleParams::TireLongStiffPerUnitGravity;

        SuspData[i].mSpringStrength = VehicleParams::SuspSpringStrength;
        SuspData[i].mSpringDamperRate = VehicleParams::SuspSpringDamperRate;
        SuspData[i].mMaxCompression = VehicleParams::SuspMaxCompression;
        SuspData[i].mMaxDroop = VehicleParams::SuspMaxDroop;
        SuspData[i].mSprungMass = VehicleParams::ChassisMass * VehicleParams::SuspSprungMassFactor;
    }

    for (int i = 0; i < 4; ++i)
    {
        PhysXWheelsSimData->setWheelData(i, WheelData[i]);
        PhysXWheelsSimData->setTireData(i, TireData[i]);
        PhysXWheelsSimData->setSuspensionData(i, SuspData[i]);
        PhysXWheelsSimData->setSuspTravelDirection(i, SuspTravelDir_PhysXVehicleLocal);
        PhysXWheelsSimData->setWheelCentreOffset(i, WheelCenterOffsets_PhysXVehicleLocal[i]);
        PhysXWheelsSimData->setSuspForceAppPointOffset(i, WheelCenterOffsets_PhysXVehicleLocal[i]);
        PhysXWheelsSimData->setTireForceAppPointOffset(i, WheelCenterOffsets_PhysXVehicleLocal[i]);
        PhysXWheelsSimData->setWheelShapeMapping(i, i); // 휠 지오메트리 셰이프 인덱스 매핑
    }

    // 2. 엔진, 기어, 클러치, 디퍼렌셜 데이터 (PxVehicleDriveSimData4W)
    PhysXDriveSimData4W = new physx::PxVehicleDriveSimData4W();

    // EngineData
    physx::PxVehicleEngineData EngineData;
    physx::PxFixedSizeLookupTable<8> TorqueCurveDataPoints;
    TorqueCurveDataPoints.addPair(0.0f, 0.8f);
    TorqueCurveDataPoints.addPair(VehicleParams::EngineMaxOmega * 0.33f, 1.0f);
    TorqueCurveDataPoints.addPair(VehicleParams::EngineMaxOmega * 0.66f, 0.9f);
    TorqueCurveDataPoints.addPair(VehicleParams::EngineMaxOmega * 1.0f, 0.7f);
    EngineData.mTorqueCurve = TorqueCurveDataPoints; // 값 복사
    EngineData.mMaxOmega = VehicleParams::EngineMaxOmega;
    EngineData.mPeakTorque = VehicleParams::EnginePeakTorque;
    EngineData.mMOI = 1.0f;
    EngineData.mDampingRateFullThrottle = 0.15f;
    EngineData.mDampingRateZeroThrottleClutchEngaged = 0.5f;
    EngineData.mDampingRateZeroThrottleClutchDisengaged = 0.05f;
    PhysXDriveSimData4W->setEngineData(EngineData);

    // GearsData
    physx::PxVehicleGearsData GearsData;
    GearsData.mSwitchTime = 0.2f;
    GearsData.mRatios[physx::PxVehicleGearsData::eREVERSE] = VehicleParams::GearRatioReverse;
    GearsData.mRatios[physx::PxVehicleGearsData::eNEUTRAL] = 0.0f;
    for (physx::PxU32 i = 0; i < VehicleParams::NumForwardGears; ++i)
    {
        if ((physx::PxVehicleGearsData::eFIRST + i) < physx::PxVehicleGearsData::eGEARSRATIO_COUNT)
        {
            GearsData.mRatios[physx::PxVehicleGearsData::eFIRST + i] = VehicleParams::GearRatioFirst / (physx::PxReal)(i + 1);
        }
    }
    GearsData.mFinalRatio = 4.0f;
    PhysXDriveSimData4W->setGearsData(GearsData);

    // ClutchData
    physx::PxVehicleClutchData ClutchData;
    ClutchData.mStrength = VehicleParams::ClutchStrength;
    ClutchData.mAccuracyMode = physx::PxVehicleClutchAccuracyMode::eBEST_POSSIBLE;
    PhysXDriveSimData4W->setClutchData(ClutchData);

    // DifferentialData
    physx::PxVehicleDifferential4WData DiffData;
    DiffData.mType = VehicleParams::DiffType;
    DiffData.mFrontRearSplit = VehicleParams::DiffFrontRearBias;
    DiffData.mFrontLeftRightSplit = VehicleParams::DiffFrontLeftRightBias;
    DiffData.mRearLeftRightSplit = VehicleParams::DiffRearLeftRightBias;
    PhysXDriveSimData4W->setDiffData(DiffData);

    // AckermannGeometryData (PhysX 차량 로컬 좌표계: X-Fwd, Z-Up, Y-Left)
    physx::PxVehicleAckermannGeometryData AckermannData;
    AckermannData.mAccuracy = 1.0f;
    // AxleSeparation: PhysX 차량 로컬 X축 기준 앞뒤 차축 간 거리
    AckermannData.mAxleSeparation = PxAbs(WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_LEFT].x - WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_LEFT].x);
    // FrontWidth/RearWidth: PhysX 차량 로컬 Y축 기준 좌우 바퀴 간 거리
    AckermannData.mFrontWidth = PxAbs(WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_LEFT].y - WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_RIGHT].y);
    AckermannData.mRearWidth = PxAbs(WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_LEFT].y - WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_RIGHT].y);
    PhysXDriveSimData4W->setAckermannGeometryData(AckermannData);
}

void ACarActor::InitializeCarPhysicsInternal()
{
    if (bIsPhysXInitialized) return;

    physx::PxPhysics* PhysXSDK = GPhysics;
    physx::PxScene* PhysXScene = GScene;
    physx::PxCooking* PhysXCooking = GCooking;
    physx::PxMaterial* DefaultChassisMaterial = GMaterial;

    if (!PhysXSDK || !PhysXScene || !PhysXCooking || !DefaultChassisMaterial)
    {
        return;
    }

    // 1. 현재 액터의 위치와 회전을 가져와 PxTransform 생성
    FVector CurrentLocation = GetActorLocation();
    FQuat CurrentRotation = GetActorRotation().Quaternion(); // FRotator를 FQuat으로 변환

    physx::PxTransform InitialGlobalPose_PhysX(
        physx::PxVec3(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z),
        physx::PxQuat(CurrentRotation.X, CurrentRotation.Y, CurrentRotation.Z, CurrentRotation.W)
    );

    // 2. CreateChassisPhysXActor_Internal 호출
    CreateChassisPhysXActor_Internal(PhysXSDK, PhysXCooking, DefaultChassisMaterial, InitialGlobalPose_PhysX);
    if (!PhysXChassisActor) return; // 섀시 생성 실패 시 중단

    // 3. CreateVehiclePhysXSimulationData_Internal 호출
    //    휠 오프셋 계산 (엔진 Z-up, X-Fwd, Y-Right -> PhysX 차량 로컬 X-Fwd, Z-Up, Y-Left)
    FVector ChassisHalfSizeVisual_Engine = (AABBMax - AABBMin) * 0.5f;
    physx::PxVec3 WheelCenterOffsets_PhysXVehicleLocal[4];
    const float WheelZOffset_PhysXVehicleLocal = -ChassisHalfSizeVisual_Engine.Z + VehicleParams::WheelRadius * 0.75f;
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_RIGHT] = physx::PxVec3(ChassisHalfSizeVisual_Engine.X * 0.8f, -ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eFRONT_LEFT] = physx::PxVec3(ChassisHalfSizeVisual_Engine.X * 0.8f, ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_RIGHT] = physx::PxVec3(-ChassisHalfSizeVisual_Engine.X * 0.8f, -ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    WheelCenterOffsets_PhysXVehicleLocal[physx::PxVehicleDrive4WWheelOrder::eREAR_LEFT] = physx::PxVec3(-ChassisHalfSizeVisual_Engine.X * 0.8f, ChassisHalfSizeVisual_Engine.Y * 0.9f, WheelZOffset_PhysXVehicleLocal);
    CreateVehiclePhysXSimulationData_Internal(WheelCenterOffsets_PhysXVehicleLocal);
    if (!PhysXWheelsSimData || !PhysXDriveSimData4W) return; // 데이터 생성 실패 시 중단

    // 4. PxVehicleDrive4W 객체 생성 및 설정
    PhysXVehicleDrive4W = PxVehicleDrive4W::allocate(4);
    if (!PhysXVehicleDrive4W) // allocate 실패 시 먼저 체크
    {
        // 이전에 생성된 PhysXWheelsSimData, PhysXDriveSimData4W, PhysXChassisActor 등을 여기서 해제해야 할 수 있음
        if (PhysXWheelsSimData) { PhysXWheelsSimData->free(); PhysXWheelsSimData = nullptr; }
        if (PhysXDriveSimData4W) { delete PhysXDriveSimData4W; PhysXDriveSimData4W = nullptr; }
        if (PhysXChassisActor) { PhysXChassisActor->release(); PhysXChassisActor = nullptr; } // 씬에 추가되기 전이므로 바로 release
        return;
    }

    PhysXVehicleDrive4W->setup(PhysXSDK, PhysXChassisActor, *PhysXWheelsSimData, *PhysXDriveSimData4W, 0);

    // 5. 씬에 섀시 액터 추가 
    if (PhysXChassisActor && PhysXScene && !bIsInScene) // bIsInScene은 여기서 관리
    {
        PhysXScene->addActor(*PhysXChassisActor);
        bIsInScene = true;
    }

    // 6. 초기 상태 설정
    PhysXVehicleDrive4W->setToRestState();
    PhysXVehicleDrive4W->mDriveDynData.setUseAutoGears(true);

    bIsPhysXInitialized = true;
}

void ACarActor::UpdateCarPhysicsStateInternal(float DeltaTime)
{
    if (!bIsPhysXInitialized || !PhysXVehicleDrive4W || !GTireFrictionPairs || !this->PhysXWheelsSimData) return;

    physx::PxScene* PhysXScene = GScene;
    if (!PhysXScene) return;

    // 1. PxRaycastQueryResult -> PxWheelQueryResult 변환
    const PxVehicleWheelsSimData& wheelsSimData = *this->PhysXWheelsSimData;
    for (physx::PxU32 i = 0; i < 4; ++i)
    {
        const physx::PxRaycastQueryResult& rawHitResult = RawRaycastResultsBuffer[i];
        physx::PxWheelQueryResult& processedWheelResult = InternalWheelQueryResults[i]; // 이름 변경 m_InternalWheelQueryResults -> InternalWheelQueryResults

        // localPose는 PxVehicleUpdates가 채워주므로 기본값으로 설정하거나,
        // 이전 프레임의 시각화 값을 사용할 수 있음. 여기서는 기본값.
        processedWheelResult.localPose = physx::PxTransform(physx::PxIdentity);

        processedWheelResult.suspLineStart = wheelsSimData.getWheelCentreOffset(i);
        processedWheelResult.suspLineDir = wheelsSimData.getSuspTravelDirection(i);
        int Test = rawHitResult.getNbAnyHits();
        if (rawHitResult.getNbAnyHits() > 0)
        {
            const physx::PxRaycastHit& hit = rawHitResult.getAnyHit(0);
            processedWheelResult.suspLineLength = hit.distance;
            processedWheelResult.isInAir = false;
            processedWheelResult.tireContactPoint = hit.position;
            processedWheelResult.tireContactNormal = hit.normal;
            processedWheelResult.tireContactShape = const_cast<physx::PxShape*>(hit.shape);
            processedWheelResult.tireContactActor = const_cast<physx::PxRigidActor*>(hit.actor);
        }
        else
        {
            processedWheelResult.isInAir = true;
            const PxVehicleSuspensionData& currentSuspData = wheelsSimData.getSuspensionData(i);
            const PxVehicleWheelData& currentWheelData = wheelsSimData.getWheelData(i);
            processedWheelResult.suspLineLength = currentSuspData.mMaxDroop + currentSuspData.mMaxCompression + currentWheelData.mRadius;
            processedWheelResult.tireContactPoint = physx::PxVec3(PxZero);
            processedWheelResult.tireContactNormal = wheelsSimData.getSuspTravelDirection(i) * -1.0f;
            processedWheelResult.tireContactShape = nullptr;
            processedWheelResult.tireContactActor = nullptr;
        }

        float currentRawSteerInput = PhysXVehicleDrive4W->mDriveDynData.getAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT);
        if (wheelsSimData.getWheelData(i).mMaxSteer != 0.0f) {
            processedWheelResult.steerAngle = currentRawSteerInput * wheelsSimData.getWheelData(i).mMaxSteer;
        }
        else {
            processedWheelResult.steerAngle = 0.0f;
        }
    }

    // 2. 차량 물리 상태 업데이트 (PxVehicleUpdates)
    physx::PxVehicleWheels* VehiclesToUpdate[1] = { PhysXVehicleDrive4W };
    PxVehicleUpdates(DeltaTime, PhysXScene->getGravity(), *GTireFrictionPairs, 1, VehiclesToUpdate, VehicleWheelQueryStateForUpdates);
}

void ACarActor::UpdateCarVisualsInternal()
{
    if (!bIsPhysXInitialized || !PhysXChassisActor || !PhysXVehicleDrive4W) return;

    // 섀시 업데이트
    const physx::PxTransform ChassisGlobalPose_Engine = PhysXChassisActor->getGlobalPose();
    SetActorLocation(FVector(ChassisGlobalPose_Engine.p.x, ChassisGlobalPose_Engine.p.y, ChassisGlobalPose_Engine.p.z));
    SetActorRotation(FRotator(FQuat(ChassisGlobalPose_Engine.q.x, ChassisGlobalPose_Engine.q.y, ChassisGlobalPose_Engine.q.z, ChassisGlobalPose_Engine.q.w)));

    // 휠 업데이트
    for (physx::PxU32 i = 0; i < 4; ++i)
    {
        if (Wheels[i]) // Wheels 멤버 변수 사용
        {
            // VehicleWheelQueryStateForUpdates[0].wheelQueryResults는 InternalWheelQueryResults를 가리킴
            const physx::PxTransform WheelLocalPose_PhysXVehicle = InternalWheelQueryResults[i].localPose; // 또는 VehicleWheelQueryStateForUpdates[0].wheelQueryResults[i].localPose
            const physx::PxTransform WheelGlobalPose_Engine = ChassisGlobalPose_Engine * WheelLocalPose_PhysXVehicle;

            Wheels[i]->SetWorldLocation(FVector(WheelGlobalPose_Engine.p.x, WheelGlobalPose_Engine.p.y, WheelGlobalPose_Engine.p.z));
            Wheels[i]->SetWorldRotation(FRotator(FQuat(WheelGlobalPose_Engine.q.x, WheelGlobalPose_Engine.q.y, WheelGlobalPose_Engine.q.z, WheelGlobalPose_Engine.q.w)));
        }
    }
}

void ACarActor::ShutdownCarPhysicsInternal()
{
    if (!bIsPhysXInitialized)
    {
        return;
    }

    physx::PxScene* PhysXScene = GScene;

    // 1. 씬에서 섀시 액터 제거 (RemoveFromScene 함수 로직 통합)
    if (PhysXChassisActor && PhysXScene && bIsInScene)
    {
        PhysXScene->removeActor(*PhysXChassisActor);
        bIsInScene = false;
    }

    // 2. PhysX 객체들 해제
    if (PhysXVehicleDrive4W) {
        PhysXVehicleDrive4W->free();
        PhysXVehicleDrive4W = nullptr;
    }
    if (PhysXWheelsSimData) {
        PhysXWheelsSimData->free();
        PhysXWheelsSimData = nullptr;
    }
    if (PhysXDriveSimData4W) {
        delete PhysXDriveSimData4W; // new로 생성했으므로
        PhysXDriveSimData4W = nullptr;
    }
    if (PhysXChassisActor) {
        PhysXChassisActor->release(); // 씬에서 제거 후 release
        PhysXChassisActor = nullptr;
    }

    bIsPhysXInitialized = false;
}

void ACarActor::AddToScene(physx::PxScene* InPhysXScene)
{
    if (PhysXChassisActor && !bIsInScene)
    {
        InPhysXScene->addActor(*PhysXChassisActor);
        bIsInScene = true;
    }
}

void ACarActor::RemoveFromScene(physx::PxScene* InPhysXScene)
{
    if (PhysXChassisActor && bIsInScene)
    {
        InPhysXScene->removeActor(*PhysXChassisActor);
        bIsInScene = false;
    }
}

void ACarActor::ProcessPlayerInput(const physx::PxVehicleDrive4WRawInputData& RawInputData)
{
    if (!PhysXVehicleDrive4W) return;

    if (!PhysXVehicleDrive4W->mDriveDynData.getUseAutoGears())
    {
        if (RawInputData.getGearUp())
        {
            PhysXVehicleDrive4W->mDriveDynData.startGearChange(PhysXVehicleDrive4W->mDriveDynData.getTargetGear() + 1);
        }
        else if (RawInputData.getGearDown())
        {
            PhysXVehicleDrive4W->mDriveDynData.startGearChange(PhysXVehicleDrive4W->mDriveDynData.getTargetGear() - 1);
        }
    }

    PhysXVehicleDrive4W->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL, RawInputData.getAnalogAccel());
    PhysXVehicleDrive4W->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT, RawInputData.getAnalogSteer()); // 양수: 오른쪽 조향
    PhysXVehicleDrive4W->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE, RawInputData.getAnalogBrake());
    PhysXVehicleDrive4W->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_HANDBRAKE, RawInputData.getAnalogHandbrake());
}

void ACarActor::PerformSuspensionRaycasts(physx::PxScene* InPhysXScene)
{
    if (!PhysXVehicleDrive4W || !GSuspensionRaycastBatchQuery) return;

    physx::PxVehicleWheels* Vehicles[1] = { PhysXVehicleDrive4W };
    bool PerformRaycast[1] = { true };
    const physx::PxU32 NumWheelsToRaycast = 4; // 이 차량의 휠 개수

    PxVehicleSuspensionRaycasts(
        GSuspensionRaycastBatchQuery,
        1,                          // nbVehicles
        Vehicles,
        NumWheelsToRaycast,         // nbSceneQueryResults (이 차량의 휠 개수)
        RawRaycastResultsBuffer,    // sceneQueryResults (PxRaycastQueryResult*)
        PerformRaycast                        // vehiclesToRaycast (선택적, 모든 차량 레이캐스트 시 NULL)
    );
}

void ACarActor::UpdateVehiclePhysicsState(float DeltaTime, const physx::PxVec3& Gravity_Engine)
{
    if (!PhysXVehicleDrive4W || !GTireFrictionPairs || !this->PhysXWheelsSimData) // this->PhysXWheelsSimData Null 체크 추가
    {
        // LogError or assert
        return;
    }

    // PxVehicleWheelsSimData에 접근 (ACarActor의 멤버인 PhysXWheelsSimData 사용)
    const PxVehicleWheelsSimData& wheelsSimData = *this->PhysXWheelsSimData;

    // --- PxRaycastQueryResult (RawRaycastResultsBuffer) 에서
    // --- PxWheelQueryResult (m_InternalWheelQueryResults) 로 데이터 변환/채우기 ---
    for (physx::PxU32 i = 0; i < 4; ++i)
    {
        const physx::PxRaycastQueryResult& rawHitResult = RawRaycastResultsBuffer[i];
        physx::PxWheelQueryResult& processedWheelResult = InternalWheelQueryResults[i];

        // 1. localPose:
        // PxVehicleUpdates가 이 값을 계산하고 채워줍니다.
        // 호출 전에 특별히 설정할 필요는 없거나, PxTransform(PxIdentity)와 같이
        // 기본적인 값으로 초기화해둘 수 있습니다.
        // 만약 이전 프레임의 상태를 반영해야 한다면, 해당 값을 가져와야 합니다.
        // 여기서는 PxVehicleUpdates가 계산한다고 가정하고, 기본값으로 둡니다.
        // (또는 이전 프레임 UpdateVisuals에서 사용한 값을 임시 저장했다가 가져올 수 있음)
        processedWheelResult.localPose = physx::PxTransform(physx::PxIdentity); // 또는 이전 프레임 값

        // 2. 서스펜션 라인 정보
        processedWheelResult.suspLineStart = wheelsSimData.getWheelCentreOffset(i);
        processedWheelResult.suspLineDir = wheelsSimData.getSuspTravelDirection(i);

        // 3. 레이캐스트 히트 정보 처리
        if (rawHitResult.getNbAnyHits() > 0)
        {
            const physx::PxRaycastHit& hit = rawHitResult.getAnyHit(0);
            processedWheelResult.suspLineLength = hit.distance;
            processedWheelResult.isInAir = false;
            processedWheelResult.tireContactPoint = hit.position;
            processedWheelResult.tireContactNormal = hit.normal;
            processedWheelResult.tireContactShape = const_cast<physx::PxShape*>(hit.shape);
            processedWheelResult.tireContactActor = const_cast<physx::PxRigidActor*>(hit.actor);
        }
        else // 공중에 뜬 경우
        {
            processedWheelResult.isInAir = true;

            // PxVehicleSuspensionData 에서 MaxDroop, MaxCompression 값을 가져옴
            const PxVehicleSuspensionData& currentSuspData = wheelsSimData.getSuspensionData(i);
            const PxVehicleWheelData& currentWheelData = wheelsSimData.getWheelData(i);

            processedWheelResult.suspLineLength = currentSuspData.mMaxDroop + currentSuspData.mMaxCompression + currentWheelData.mRadius;

            processedWheelResult.tireContactPoint = physx::PxVec3(PxZero);
            processedWheelResult.tireContactNormal = wheelsSimData.getSuspTravelDirection(i) * -1.0f;
            processedWheelResult.tireContactShape = nullptr;
            processedWheelResult.tireContactActor = nullptr;
        }

        // 4. steerAngle 설정 (이것은 PxVehicleUpdates에 입력으로 제공)
        float currentRawSteerInput = PhysXVehicleDrive4W->mDriveDynData.getAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT);
        if (wheelsSimData.getWheelData(i).mMaxSteer != 0.0f) // 앞바퀴인 경우
        {
            processedWheelResult.steerAngle = currentRawSteerInput * wheelsSimData.getWheelData(i).mMaxSteer;
        }
        else // 뒷바퀴인 경우
        {
            processedWheelResult.steerAngle = 0.0f;
        }
            
        // 나머지 PxWheelQueryResult 필드(suspJounce, suspSpringForce, tireFriction 등)는
        // PxVehicleUpdates 함수가 계산하여 채워줍니다.
    }
    // --- 데이터 변환 끝 ---

    physx::PxVehicleWheels* Vehicles[1] = { PhysXVehicleDrive4W };
    PxVehicleUpdates(DeltaTime, Gravity_Engine, *GTireFrictionPairs, 1, Vehicles, VehicleWheelQueryStateForUpdates);
}

void ACarActor::UpdateVisuals()
{
    if (!PhysXChassisActor || !PhysXVehicleDrive4W || !bIsInScene) return;

    // 1. 섀시(BodyComponent) 업데이트
    // PhysX 액터의 글로벌 포즈는 엔진 월드 좌표계 (Z-up, X-Fwd, Y-Right)
    const physx::PxTransform ChassisGlobalPose_Engine = PhysXChassisActor->getGlobalPose();
    FVector NewBodyLocation_Engine(ChassisGlobalPose_Engine.p.x, ChassisGlobalPose_Engine.p.y, ChassisGlobalPose_Engine.p.z);
    FQuat NewBodyRotation_Engine(ChassisGlobalPose_Engine.q.x, ChassisGlobalPose_Engine.q.y, ChassisGlobalPose_Engine.q.z, ChassisGlobalPose_Engine.q.w);

    Body->SetWorldLocation(NewBodyLocation_Engine);   // 엔진 API
    Body->SetWorldRotation(NewBodyRotation_Engine); // 엔진 API

    // 2. 각 휠(WheelComponents) 업데이트
    for (physx::PxU32 i = 0; i < 4; ++i)
    {
        // PxVehicleUpdates 이후, VehicleWheelQueryStateForUpdates[0].wheelQueryResults[i].localPose 에는
        // 업데이트된 휠의 (섀시 기준) 로컬 포즈가 들어있음.
        const physx::PxTransform WheelLocalPose_PhysXVehicle = VehicleWheelQueryStateForUpdates[0].wheelQueryResults[i].localPose;
        const physx::PxTransform WheelGlobalPose_Engine = ChassisGlobalPose_Engine * WheelLocalPose_PhysXVehicle;

        FVector NewWheelLocation_Engine(WheelGlobalPose_Engine.p.x, WheelGlobalPose_Engine.p.y, WheelGlobalPose_Engine.p.z);
        FQuat NewWheelRotation_Engine(WheelGlobalPose_Engine.q.x, WheelGlobalPose_Engine.q.y, WheelGlobalPose_Engine.q.z, WheelGlobalPose_Engine.q.w);

        Wheels[i]->SetWorldLocation(NewWheelLocation_Engine);
        Wheels[i]->SetWorldRotation(NewWheelRotation_Engine);
    }
}

void ACarActor::BeginPlay()
{
    Super::BeginPlay();

    // 차량 공통 데이터 설정
    if (!SetupCommonVehicleDataGlobal(*GPhysics, *GScene, *GMaterial))
    {
        return;
    }
    // PhysX 초기화
    if (!bIsPhysXInitialized)
    {
        InitializeCarPhysicsInternal();
    }
}

void ACarActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);


    // AActor::Tick은 PhysX 시뮬레이션의 특정 단계를 호출하기에 이상적인 위치가 아님.
    // EngineCore에서 PxScene::simulate() 전후로 ACarActor의 특정 함수들을 호출해야 함.
    // (예: EngineCore::Tick() -> ... ProcessPlayerInput() ... PerformSuspensionRaycasts() ...
    //      scene.simulate() ... scene.fetchResults() ... UpdateCarPhysicsStateInternal() ... UpdateCarVisualsInternal() ...)
    //
    // 만약 EngineCore의 제어 없이 Tick에서 모든 것을 처리해야 한다면, 이는 부정확한 시뮬레이션을 초래할 수 있음.
    // 여기서는 EngineCore가 올바른 순서로 아래 함수들을 호출해준다고 가정하고,
    // Tick은 게임 로직 등 다른 용도로 사용될 수 있음을 시사.
    // 또는, 임시로 Tick에서 호출하도록 하되, 나중에 EngineCore로 옮기는 것을 고려.

    // 아래는 "만약 EngineCore가 이 함수들을 직접 호출해주지 않고,
    // Tick에서 모든 것을 해야 한다면"의 (부정확할 수 있는) 예시입니다.
    if (bIsPhysXInitialized)
    {
        // 1. 입력 처리 (외부에서 이미 호출되었거나, 여기서 직접 처리)
        // ProcessPlayerInput(...); // <- PxVehicleDrive4WRawInputData 필요

        // 2. 레이캐스트 (simulate 전에 호출되어야 함)
         PerformSuspensionRaycasts(GScene); // <- 이것은 simulate 전에 호출되어야 함

        // !!! 여기서 PxScene::simulate() 와 fetchResults()가 호출되었다고 가정 !!!
        // (실제로는 EngineCore에서 이루어져야 함)
         GScene->simulate(DeltaTime);
         GScene->fetchResults(true);

        // 3. 물리 상태 업데이트 (fetchResults 후에 호출되어야 함)
        UpdateCarPhysicsStateInternal(DeltaTime);

        // 4. 시각화 업데이트
        UpdateCarVisualsInternal();
    }
}

void ACarActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ShutdownCarPhysicsInternal();
    ReleaseCommonVehicleDataGlobal();
}
