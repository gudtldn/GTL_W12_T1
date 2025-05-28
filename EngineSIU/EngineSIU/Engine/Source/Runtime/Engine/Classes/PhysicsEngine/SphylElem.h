#pragma once
#include "ShapeElem.h"
#include "UObject/ObjectMacros.h"


struct FKSphylElem : public FKShapeElem
{
    DECLARE_STRUCT(FKSphylElem, FKShapeElem)

public:
    FKSphylElem() = default;
    virtual ~FKSphylElem() override = default;

    FKSphylElem(const FKSphylElem&) = default;
    FKSphylElem& operator=(const FKSphylElem&) = default;
    FKSphylElem(FKSphylElem&&) = default;
    FKSphylElem& operator=(FKSphylElem&&) = default;

public:
    /** Position of the capsule's origin */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Capsule", .ToolTip = "Position of the capsule's origin" }),
        FVector, Center, ;
    )

    /** Rotation of the capsule */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Capsule", .ToolTip = "Rotation of the capsule", .ClampMin = -360.0f, .ClampMax = 360.0f }),
        FRotator, Rotation, ;
    )

    /** Radius of the capsule */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Capsule", .ToolTip = "Radius of the capsule" }),
        float, Radius, ;
    )

    /** This is of line-segment ie. add Radius to both ends to find total length. */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Capsule", .ToolTip = "This is of line-segment ie. add Radius to both ends to find total length." }),
        float, Length, ;
    )
};
