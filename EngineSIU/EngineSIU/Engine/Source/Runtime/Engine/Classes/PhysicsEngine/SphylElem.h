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

    float Radius;
    float HalfHeight;
};
