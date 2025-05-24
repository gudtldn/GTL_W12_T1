#pragma once
#include "BodyInstanceCore.h"

class UPrimitiveComponent;


struct FBodyInstance : FBodyInstanceCore
{
    DECLARE_STRUCT(FBodyInstance, FBodyInstanceCore)

public:
    FBodyInstance() = default;
    virtual ~FBodyInstance() override = default;

    FBodyInstance(const FBodyInstance&) = default;
    FBodyInstance& operator=(const FBodyInstance&) = default;
    FBodyInstance(FBodyInstance&&) = default;
    FBodyInstance& operator=(FBodyInstance&&) = default;

public:
    TWeakObjectPtr<UPrimitiveComponent> OwnerComponent;
};
