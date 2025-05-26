#pragma once
#include "BoxElem.h"
#include "ConvexElem.h"
#include "SphereElem.h"
#include "SphylElem.h"
#include "UObject/ObjectMacros.h"


struct FKAggregateGeom
{
    DECLARE_STRUCT(FKAggregateGeom)

public:
    FKAggregateGeom() = default;
    virtual ~FKAggregateGeom() = default;

    FKAggregateGeom(const FKAggregateGeom&) = default;
    FKAggregateGeom& operator=(const FKAggregateGeom&) = default;
    FKAggregateGeom(FKAggregateGeom&&) = default;
    FKAggregateGeom& operator=(FKAggregateGeom&&) = default;

public:
    // 구체 콜리전
    UPROPERTY(
        EditAnywhere | EditFixedSize, ({ .Category = "Aggregate Geometry", .DisplayName = "Spheres" }),
        TArray<FKSphereElem>, SphereElems, ;
    )

    // 박스 콜리전
    UPROPERTY(
        EditAnywhere | EditFixedSize, ({ .Category = "Aggregate Geometry", .DisplayName = "Boxes" }),
        TArray<FKBoxElem>, BoxElems, ;
    )

    // 캡슐 콜리전
    UPROPERTY(
        EditAnywhere | EditFixedSize, ({ .Category = "Aggregate Geometry", .DisplayName = "Capsules" }),
        TArray<FKSphylElem>, SphylElems, ;
    )

    // 컨벡스 콜리전
    UPROPERTY(
        EditAnywhere | EditFixedSize, ({ .Category = "Aggregate Geometry", .DisplayName = "Convex Elements" }),
        TArray<FKConvexElem>, ConvexElems, ;
    )
};
