#pragma once

#include "UObject/ObjectMacros.h"
#include "Math/Transform.h"
#include "PxPhysicsAPI.h" 
#include "ConstraintSetupCore.h"

struct FAngularConstraintLimit
{
    DECLARE_STRUCT(FAngularConstraintLimit)

public:

    float TwistLimitAngle = 45.f; //x
    float Swing1LimitAngle = 45.f;
    float Swing2LimitAngle = 45.f;
};

class UConstraintSetup : public UConstraintSetupCore
{
    DECLARE_CLASS(UConstraintSetup, UConstraintSetupCore)

public:
    UConstraintSetup()
        : JointName(NAME_None)
        , ConstraintBone1(NAME_None)
        , ConstraintBone2(NAME_None)
        , LocalFrame1(physx::PxTransform())
        , LocalFrame2(physx::PxTransform())
        , bDisableCollisionBetweenConstrainedBodies(false)
    {    }

    FName JointName; 
    FName ConstraintBone1;
    FName ConstraintBone2;

    physx::PxTransform LocalFrame1;
    physx::PxTransform LocalFrame2;

    FAngularConstraintLimit AngularLimits;

    bool bDisableCollisionBetweenConstrainedBodies;
};
