#pragma once
#include "UObject/ObjectMacros.h"

#include "PhysicsEngine/BodyInstance.h"

class USkeletalMeshComponent;
class UPrimitiveComponent;
class UConstraintSetup;

class UConstraintInstance : public UConstraintSetup
{
    DECLARE_CLASS(UConstraintInstance, UConstraintSetup)

public:
    UConstraintInstance() = default;
    virtual ~UConstraintInstance() override = default;

    UConstraintInstance(const UConstraintInstance&) = default;
    UConstraintInstance& operator=(const UConstraintInstance&) = default;
    UConstraintInstance(UConstraintInstance&&) = default;
    UConstraintInstance& operator=(UConstraintInstance&&) = default;

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

private:
    FBodyInstance* Body1 = nullptr;
    FBodyInstance* Body2 = nullptr;
    physx::PxJoint* Joint = nullptr;
    bool bInitialized = false; 
};
