#pragma once
#include "UObject/ObjectMacros.h"

class UBodySetupCore;


struct FBodyInstanceCore
{
    DECLARE_STRUCT(FBodyInstanceCore)

public:
    FBodyInstanceCore() = default;
    virtual ~FBodyInstanceCore() = default;

    FBodyInstanceCore(const FBodyInstanceCore&) = default;
    FBodyInstanceCore& operator=(const FBodyInstanceCore&) = default;
    FBodyInstanceCore(FBodyInstanceCore&&) = default;
    FBodyInstanceCore& operator=(FBodyInstanceCore&&) = default;

public:
    TWeakObjectPtr<UBodySetupCore> BodySetup;
};
