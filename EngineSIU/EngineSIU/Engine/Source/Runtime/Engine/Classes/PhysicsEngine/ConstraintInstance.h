#pragma once
#include "UObject/ObjectMacros.h"

#include "PxPhysicsAPI.h" 
#include "PhysicsEngine/BodyInstance.h"
#include "ConstraintInstanceCore.h"

class USkeletalMeshComponent;
class UPrimitiveComponent;
class UConstraintSetup;

struct FConstraintInstance : FConstraintInstanceCore
{
    DECLARE_STRUCT(FConstraintInstance, FConstraintInstanceCore)

public:
    FConstraintInstance() = default;
    virtual ~FConstraintInstance() override = default;

    FConstraintInstance(const FConstraintInstance&) = default;
    FConstraintInstance& operator=(const FConstraintInstance&) = default;
    FConstraintInstance(FConstraintInstance&&) = default;
    FConstraintInstance& operator=(FConstraintInstance&&) = default;

public:
    UPROPERTY(
        VisibleAnywhere, { .Category = "Constraint" },
        FName, JointName, ;
    )

public:
    // TODO: Implements This
    void TermConstraint();

    physx::PxTransform ToPxTransform(const FTransform& UnrealTransform);

    void InitConstraint(const UConstraintSetup* Setup, FBodyInstance* InBody1, FBodyInstance* InBody2, USkeletalMeshComponent* OwnerComp, bool bInSimulatePhysics);

    FBodyInstance* Body1 = nullptr;
    FBodyInstance* Body2 = nullptr;
    physx::PxJoint* Joint = nullptr;
    bool bInitialized = false; 
};
