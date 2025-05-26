#pragma once
#include "UObject/ObjectMacros.h"


struct FConstraintInstanceBase
{
    DECLARE_STRUCT(FConstraintInstanceBase)

public:
    FConstraintInstanceBase() = default;
    virtual ~FConstraintInstanceBase() = default;

    FConstraintInstanceBase(const FConstraintInstanceBase&) = default;
    FConstraintInstanceBase& operator=(const FConstraintInstanceBase&) = default;
    FConstraintInstanceBase(FConstraintInstanceBase&&) = default;
    FConstraintInstanceBase& operator=(FConstraintInstanceBase&&) = default;
};

struct FConstraintInstance : public FConstraintInstanceBase
{
    DECLARE_STRUCT(FConstraintInstance, FConstraintInstanceBase)

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
};
