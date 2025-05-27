#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


class UBodySetupCore : public UObject
{
    DECLARE_CLASS(UBodySetupCore, UObject)

public:
    UBodySetupCore() = default;
    UBodySetupCore(const FName& InBoneName)
        : BoneName(InBoneName)
    { }

    virtual ~UBodySetupCore() override = default;

public:
    UPROPERTY(
        VisibleAnywhere, { .Category = "BodySetup" },
        FName, BoneName, ;
    )
};
