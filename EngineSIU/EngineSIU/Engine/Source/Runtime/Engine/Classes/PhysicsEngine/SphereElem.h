#pragma once
#include "ShapeElem.h"
#include "UObject/ObjectMacros.h"


struct FKSphereElem : public FKShapeElem
{
    DECLARE_STRUCT(FKSphereElem, FKShapeElem)

public:
    FKSphereElem() = default;
    virtual ~FKSphereElem() override = default;

    FKSphereElem(const FKSphereElem&) = default;
    FKSphereElem& operator=(const FKSphereElem&) = default;
    FKSphereElem(const FKSphereElem&&) = default;
    FKSphereElem& operator=(FKSphereElem&&) = default;
};
