#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/SkeletalMeshComponent.h" 
#include "Math/MathUtility.h"     
#include "PxPhysicsAPI.h"
#include "ConstraintSetup.h"
#include "PhysicsEngine/PhysX/PhysX.h"

using namespace physx;

void FConstraintInstance::InitConstraint(
    const UConstraintSetup* Setup,
    FBodyInstance* InBody1,
    FBodyInstance* InBody2,
    USkeletalMeshComponent* OwnerComp,
    bool bInSimulatePhysics)
{
    if (bInitialized || !FPhysX::GPhysics || !InBody1 || !InBody2 || !InBody1->RigidActor || !InBody2->RigidActor)
    {
        return;
    }

    Body1 = InBody1; 
    Body2 = InBody2;
    JointName = Setup->JointName;

    PxRigidActor* PActor1 = Body1->RigidActor;
    PxRigidActor* PActor2 = Body2->RigidActor;

    PxTransform parentActorWorldPose = PActor1->getGlobalPose();
    PxTransform childActorWorldPose = PActor2->getGlobalPose();

    PxQuat q_AxisCorrection = PxQuat(PxMat33(
        PxVec3(0.0f, 0.0f, 1.0f), // new X-axis column
        PxVec3(1.0f, 0.0f, 0.0f), // new Y-axis column
        PxVec3(0.0f, 1.0f, 0.0f)  // new Z-axis column
    ));
    q_AxisCorrection.normalize(); 
    PxTransform PLocalFrame_Child(PxVec3(0.0f), q_AxisCorrection);

    PxTransform childJointFrameInWorld = childActorWorldPose * PLocalFrame_Child;

    PxTransform PLocalFrame_Parent = parentActorWorldPose.getInverse() * childJointFrameInWorld;

    PxD6Joint* D6Joint = PxD6JointCreate(*FPhysX::GPhysics,
        PActor2,            // actor0 = 자식 (PActor2)
        PLocalFrame_Child,  // localFrame0 = 수정된 자식 기준 프레임
        PActor1,            // actor1 = 부모 (PActor1)
        PLocalFrame_Parent  // localFrame1 = 수정된 부모 기준 프레임
    );

    if (!D6Joint)
    {
        // 로그: 조인트 생성 실패
        return;
    }

    D6Joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
    D6Joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
    D6Joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

    D6Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
    D6Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
    D6Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);

    D6Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);

    float TwistLimitRad = FMath::DegreesToRadians(Setup->AngularLimits.TwistLimitAngle);
    D6Joint->setTwistLimit(PxJointAngularLimitPair(-TwistLimitRad, TwistLimitRad, PxSpring(0, 0)));;

    float Swing1LimitRad = FMath::DegreesToRadians(Setup->AngularLimits.Swing1LimitAngle);
    float Swing2LimitRad = FMath::DegreesToRadians(Setup->AngularLimits.Swing2LimitAngle);
    D6Joint->setSwingLimit(PxJointLimitCone(Swing1LimitRad, Swing2LimitRad, PxSpring(0, 0)));
    
    D6Joint->setProjectionLinearTolerance(0.5f);
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
