#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UBodySetup;


class UPhysicsAsset : public UObject
{
    DECLARE_CLASS(UPhysicsAsset, UObject)

public:
    UPhysicsAsset() = default;
    virtual ~UPhysicsAsset() override = default;

public:
    UPROPERTY(
        TArray<UBodySetup*>, BodySetup, ;
    )
};
