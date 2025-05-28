#pragma once
#include "UObject/ObjectMacros.h"

class UConstraintSetupCore;


struct FConstraintInstanceCore
{
    DECLARE_STRUCT(FConstraintInstanceCore)

public:
    FConstraintInstanceCore() = default;
    virtual ~FConstraintInstanceCore() = default;

    FConstraintInstanceCore(const FConstraintInstanceCore&) = default;
    FConstraintInstanceCore& operator=(const FConstraintInstanceCore&) = default;
    FConstraintInstanceCore(FConstraintInstanceCore&&) = default;
    FConstraintInstanceCore& operator=(FConstraintInstanceCore&&) = default;

public:
    TWeakObjectPtr<UConstraintSetupCore> ConstraintSetup;
};
