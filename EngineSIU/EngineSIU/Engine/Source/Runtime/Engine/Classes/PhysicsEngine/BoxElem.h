#pragma once
#include "ShapeElem.h"
#include "UObject/ObjectMacros.h"


struct FKBoxElem : public FKShapeElem
{
    DECLARE_STRUCT(FKBoxElem, FKShapeElem)

public:
    FKBoxElem()
        : Center(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , X(1.0f), Y(1.0f), Z(1.0f)
    {
    }

    FKBoxElem(float InScale)
        : Center(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , X(InScale), Y(InScale), Z(InScale)
    {
    }

    FKBoxElem(float InX, float InY, float InZ)
        : Center(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , X(InX), Y(InY), Z(InZ)
    {
    }

    virtual ~FKBoxElem() override = default;

    FKBoxElem(const FKBoxElem&) = default;
    FKBoxElem& operator=(const FKBoxElem&) = default;
    FKBoxElem(FKBoxElem&&) = default;
    FKBoxElem& operator=(FKBoxElem&&) = default;

public:
    /** Position of the box's origin */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Box", .ToolTip = "Position of the box's origin" }),
        FVector, Center, ;
    )

    /** Rotation of the box */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Box", .ToolTip = "Rotation of the box", .ClampMin = -360.0f, .ClampMax = 360.0f }),
        FRotator, Rotation, ;
    )

    /** Extent of the box along the y-axis */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Box", .DisplayName = "X Extent", .ToolTip = "Extent of the box along the y-axis" }),
        float, X, ;
    )

    /** Extent of the box along the y-axis */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Box", .DisplayName = "Y Extent", .ToolTip = "Extent of the box along the y-axis" }),
        float, Y, ;
    )

    /** Extent of the box along the z-axis */
    UPROPERTY(
        EditAnywhere, ({ .Category = "Box", .DisplayName = "Z Extent", .ToolTip = "Extent of the box along the z-axis" }),
        float, Z, ;
    )
};
