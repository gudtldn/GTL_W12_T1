#pragma once
#include "ShapeElem.h"
#include "UObject/ObjectMacros.h"


struct FKSphereElem : public FKShapeElem
{
    DECLARE_STRUCT(FKSphereElem, FKShapeElem)

public:
    FKSphereElem()
        : Center(FVector::ZeroVector)
        , Radius(0.0f)
    {
    }

    FKSphereElem(float InRadius)
        : Center(FVector::ZeroVector)
        , Radius(InRadius)
    {
    }
    virtual ~FKSphereElem() override = default;

    FKSphereElem(const FKSphereElem&) = default;
    FKSphereElem& operator=(const FKSphereElem&) = default;
    FKSphereElem(FKSphereElem&&) = default;
    FKSphereElem& operator=(FKSphereElem&&) = default;

public:
    /** Position of the sphere's origin */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Sphere", .ToolTip = "Position of the sphere's origin" }),
        FVector, Center, ;
    )

    /** Radius of the sphere */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Sphere", .ToolTip = "Radius of the sphere" }),
        float, Radius, ;
    )
};
