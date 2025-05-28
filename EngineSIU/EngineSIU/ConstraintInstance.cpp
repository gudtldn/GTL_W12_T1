#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/SkeletalMeshComponent.h" 
#include "Math/MathUtility.h"     
#include "PxPhysicsAPI.h"
#include "ConstraintSetup.h"

using namespace physx;

extern PxPhysics* GPhysics;

void FConstraintInstance::InitConstraint(
    const UConstraintSetup* Setup,
    FBodyInstance* InBody1,
    FBodyInstance* InBody2,
    USkeletalMeshComponent* OwnerComp,
    bool bInSimulatePhysics)
{
    if (bInitialized || !GPhysics || !InBody1 || !InBody2 || !InBody1->RigidActor || !InBody2->RigidActor)
    {
        return;
    }

    Body1 = InBody1;
    Body2 = InBody2;
    JointName = Setup->JointName;

    PxRigidActor* PActor1 = Body1->RigidActor;
    PxRigidActor* PActor2 = Body2->RigidActor;
    PxTransform PLocalFrame1 = Setup->LocalFrame1;
    PxTransform PLocalFrame2 = Setup->LocalFrame2;
    PxD6Joint* D6Joint = PxD6JointCreate(*GPhysics,
        PActor2, PLocalFrame2,
        PActor1, PLocalFrame1
    );

    if (!D6Joint)
    {
        // 로그: 조인트 생성 실패
        return;
    }

    D6Joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
    D6Joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
    D6Joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

    D6Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED); // Twist (Y-Z 평면 회전)
    D6Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED); // Swing1 (X-Z 평면 회전)
    D6Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED); // Swing2 (X-Y 평면 회전)

    float TwistLimitRad = FMath::DegreesToRadians(Setup->AngularLimits.TwistLimitAngle);
    D6Joint->setTwistLimit(PxJointAngularLimitPair(-TwistLimitRad, TwistLimitRad));

    float Swing1LimitRad = FMath::DegreesToRadians(Setup->AngularLimits.Swing1LimitAngle);
    float Swing2LimitRad = FMath::DegreesToRadians(Setup->AngularLimits.Swing2LimitAngle);
    D6Joint->setSwingLimit(PxJointLimitCone(Swing1LimitRad, Swing2LimitRad));

    //if (Setup->bDisableCollisionBetweenConstrainedBodies)
    //{
    //    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);
    //}
    //else
    //{
    //    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, true);
    //}
    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);
    D6Joint->setProjectionLinearTolerance(0.05f);
    D6Joint->setProjectionAngularTolerance(FMath::DegreesToRadians(1.0f));
    D6Joint->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);

    Joint = D6Joint;
    bInitialized = true;
}

void FConstraintInstance::TermConstraint()
{
    if (bInitialized && Joint)
    {
        if (Joint->getScene()) // 조인트가 씬에 연결되어 있는지 확인 (안전장치)
        {
            // PhysX 조인트 해제. PhysX Actor는 FBodyInstance에서 관리/해제.
        }
        Joint->release(); // PhysX 조인트 객체 자체를 해제
        Joint = nullptr;
    }
    Body1 = nullptr;
    Body2 = nullptr;
    bInitialized = false;
}
