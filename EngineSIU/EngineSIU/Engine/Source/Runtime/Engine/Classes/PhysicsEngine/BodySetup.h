#pragma once
#include "AggregateGeom.h"
#include "BodySetupCore.h"


class UBodySetup : public UBodySetupCore
{
    DECLARE_CLASS(UBodySetup, UBodySetupCore)

public:
    UBodySetup() = default;
    virtual ~UBodySetup() override = default;

public:
    UPROPERTY(
        EditAnywhere, ({ .Category = "BodySetup", .DisplayName = "Primitives" }),
        FKAggregateGeom, AggGeom, ;
    )

    UPROPERTY
    (float, Mass)
    UPROPERTY
    (bool, bOverrideMass)
    UPROPERTY
    (float, Density)
    UPROPERTY
    (float, LinearDamping)
    UPROPERTY
    (float, AngularDamping)
};
