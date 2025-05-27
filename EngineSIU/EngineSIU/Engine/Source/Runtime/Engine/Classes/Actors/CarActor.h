#pragma once
#include "GameFramework/Actor.h"
#include "PxPhysicsAPI.h"
#include "vehicle/PxVehicleSDK.h"
#include "vehicle/PxVehicleDrive4W.h"
#include "vehicle/PxVehicleDrive.h"
#include "vehicle/PxVehicleDriveNW.h"
#include "vehicle/PxVehicleComponents.h"
#include "vehicle/PxVehicleUtilSetup.h"
#include "vehicle/PxVehicleUtilControl.h"
#include "vehicle/PxVehicleTireFriction.h"

class UCubeComp;
class USphereComp;

// 쉽게 튜닝할 수 있도록 차량 파라미터 정의
namespace VehicleParams
{
    // 섀시
    const physx::PxReal ChassisMass = 1500.0f; // kg

    // 휠
    const physx::PxReal WheelMass = 20.0f;
    const physx::PxReal WheelRadius = 0.7f; // m
    const physx::PxReal WheelWidth = 0.2f;
    const physx::PxReal WheelDampingRate = 0.25f;
    const physx::PxReal MaxSteerAngleRad = physx::PxPi / 3.0f; // 약 60도

    // 서스펜션
    const physx::PxReal SuspSpringStrength = 35000.0f;
    const physx::PxReal SuspSpringDamperRate = 4500.0f;
    const physx::PxReal SuspMaxCompression = 0.3f;
    const physx::PxReal SuspMaxDroop = 0.1f;
    const physx::PxReal SuspSprungMassFactor = 0.25f;

    // 타이어 (기본값, 지면 재질에 따라 달라짐)
    const physx::PxReal TireFrictionMultiplier = 2.0f;
    const physx::PxReal TireLatStiffX = 20.0f; // 타이어 모델 단순화를 위해 사용하지 않을 수 있음
    const physx::PxReal TireLatStiffY = 18000.0f / (physx::PxPi / 180.0f); // 슬립 각도당 측면 힘
    const physx::PxReal TireLongStiffPerUnitGravity = 1000.0f; // 타이어 종방향 강성 (수직항력에 비례)


    // 엔진 (간단한 설정)
    const physx::PxReal EnginePeakTorque = 500.0f; // Nm
    const physx::PxReal EngineMaxOmega = 600.0f;   // rad/s (약 5700 RPM)

    // 기어
    const physx::PxReal GearRatioReverse = -4.0f;
    const physx::PxReal GearRatioFirst = 4.0f;
    const physx::PxU32 NumForwardGears = 4; // 1~4단

    // 클러치
    const physx::PxReal ClutchStrength = 10.0f;

    // 디퍼렌셜 (4WD Open)
    const physx::PxVehicleDifferential4WData::Enum DiffType = physx::PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_4WD;
    const physx::PxReal DiffFrontRearBias = 0.5f; // 50:50
    const physx::PxReal DiffFrontLeftRightBias = 0.5f;
    const physx::PxReal DiffRearLeftRightBias = 0.5f;
}

class ACarActor : public AActor
{
    DECLARE_CLASS(ACarActor, AActor);

public:
    ACarActor();
    virtual ~ACarActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // PhysX 차량 및 공통 데이터 초기화/해제 (애플리케이션 레벨)
    static bool InitializeVehicleSDKGlobal(physx::PxPhysics& PhysXSDK);
    static void ShutdownVehicleSDKGlobal();
    static bool SetupCommonVehicleDataGlobal(physx::PxPhysics& PhysXSDK, physx::PxScene& PhysXScene, physx::PxMaterial& DefaultGroundMaterial);
    static void ReleaseCommonVehicleDataGlobal();

    // 개별 차량 PhysX 초기화
    void InitializePhysics(physx::PxPhysics* InPhysXSDK, physx::PxCooking* InPhysXCooking, physx::PxMaterial* InDefaultChassisMaterial, const FVector& InitialPosition, const FQuat& InitialRotation);

    // 씬에 추가/제거
    void AddToScene(physx::PxScene* InPhysXScene);
    void RemoveFromScene(physx::PxScene* InPhysXScene);

    // 매 프레임 업데이트 함수
    void ProcessPlayerInput(const physx::PxVehicleDrive4WRawInputData& RawInputData);
    void PerformSuspensionRaycasts(physx::PxScene* InPhysXScene); // PxScene::simulate() 전에 호출
    void UpdateVehiclePhysicsState(float DeltaTime, const physx::PxVec3& Gravity);
    void UpdateVisuals();

    //PhyxX.cpp의 전역변수 사용
    //physx::PxPhysics* GetPhysXSDKFromWorld() const;
    //physx::PxScene* GetPhysXSceneFromWorld() const;

private:
    bool bIsPhysXInitialized; // 이 액터의 PhysX가 초기화되었는지 여부

    UCubeComp* Body = nullptr;
    USphereComp* Wheels[4] = { nullptr, nullptr, nullptr, nullptr };

    // PhysX 객체 멤버
    physx::PxRigidDynamic* PhysXChassisActor;
    physx::PxVehicleDrive4W* PhysXVehicleDrive4W;
    physx::PxVehicleWheelsSimData* PhysXWheelsSimData;    // 휠/타이어/서스펜션 데이터
    physx::PxVehicleDriveSimData4W* PhysXDriveSimData4W;  // 엔진/기어/클러치/디퍼렌셜 데이터

    // 레이캐스트 결과 버퍼
    physx::PxRaycastQueryResult     RawRaycastResultsBuffer[4];          // PxVehicleSuspensionRaycasts 가 채움
    physx::PxWheelQueryResult       InternalWheelQueryResults[4];      // <<-- PxVehicleUpdates 에 전달될 PxWheelQueryResult 데이터 저장용
    physx::PxVehicleWheelQueryResult VehicleWheelQueryStateForUpdates[1]; // <<-- InternalWheelQueryResults 를 가리키는 래퍼

    bool bIsInScene;
    FVector AABBMin, AABBMax; // 시각적 AABB 
    float WheelRadiusVisual;             // 시각적 휠 반지름

    // 내부 PhysX 객체 생성 헬퍼
    void CreateChassisPhysXActor_Internal(physx::PxPhysics* InPhysXSDK, physx::PxCooking* InPhysXCooking, physx::PxMaterial* InDefaultChassisMaterial, const physx::PxTransform& GlobalPose_Engine);
    void CreateVehiclePhysXSimulationData_Internal(const physx::PxVec3 WheelCenterOffsets_PhysXVehicleLocal[4]);

    // BeginePlay 시 PhysX 데이터 초기화
    void InitializeCarPhysicsInternal();

    // Tick 시 PhysX 업데이트
    void UpdateCarPhysicsStateInternal(float DeltaTime);
    void UpdateCarVisualsInternal(); // 물리 업데이트 이후

    // PhysX 관련 리소스 해제
    void ShutdownCarPhysicsInternal();


public:
    // 공유 데이터 접근자
    static physx::PxVehicleDrivableSurfaceToTireFrictionPairs* GetGlobalTireFrictionPairs() { return GTireFrictionPairs; }

private:
    // 모든 차량이 공유하는 PhysX 데이터 (정적 멤버)
    static physx::PxVehicleDrivableSurfaceToTireFrictionPairs* GTireFrictionPairs;
    static physx::PxBatchQuery* GSuspensionRaycastBatchQuery; // 서스펜션 레이캐스트용 배치 쿼리
    static physx::PxQueryFilterData GRaycastQueryFilterData; // 레이캐스트 필터 데이터

    // PxBatchQueryMemory에 사용될 정적 버퍼들 (모든 차량 인스턴스가 공유)
    inline static const physx::PxU32 MAX_RAYCASTS_IN_BATCH = 4; // 예시: 최대 4개의 동시 레이캐스트
    inline static const physx::PxU32 MAX_TOUCHES_PER_RAYCAST = 1; // 각 레이캐스트당 예상되는 최대 히트 수
    inline static const physx::PxU32 TOTAL_RAYCAST_TOUCH_BUFFER_SIZE = MAX_RAYCASTS_IN_BATCH * MAX_TOUCHES_PER_RAYCAST;

    static physx::PxRaycastHit GBatchQueryRaycastTouchBuffer[TOTAL_RAYCAST_TOUCH_BUFFER_SIZE];
    static physx::PxRaycastQueryResult GSharedRaycastResultBuffer[MAX_RAYCASTS_IN_BATCH];
    static physx::PxRaycastHit         GSharedRaycastTouchBuffer[TOTAL_RAYCAST_TOUCH_BUFFER_SIZE];
};
