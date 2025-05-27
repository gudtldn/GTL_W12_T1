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

    UPROPERTY(
        EditAnywhere | LuaReadWrite, ({ .Category = "Physics", .ToolTip = "Mass of the body for physics simulation", .ClampMin = 0.001f }),
        float, Mass, = 10.f;
    )
};
