#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "ConstraintSetup.h"

class UBodySetup;

class UPhysicsAsset : public UObject
{
    DECLARE_CLASS(UPhysicsAsset, UObject)

public:
    UPhysicsAsset() = default;
    virtual ~UPhysicsAsset() override = default;

public:
    UPROPERTY(
        EditAnywhere | EditInline,
        TArray<UBodySetup*>, BodySetup, ;
    )

    UPROPERTY(
        EditAnywhere | EditInline,
        TArray<UConstraintSetup*>, ConstraintSetup, ;
    )
};
