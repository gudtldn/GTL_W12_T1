#pragma once
#include "ShapeElem.h"
#include "UObject/ObjectMacros.h"


struct FKConvexElem : public FKShapeElem
{
    DECLARE_STRUCT(FKConvexElem, FKShapeElem)

public:
    FKConvexElem() = default;
    virtual ~FKConvexElem() override = default;

    FKConvexElem(const FKConvexElem&) = default;
    FKConvexElem& operator=(const FKConvexElem&) = default;
    FKConvexElem(const FKConvexElem&&) = default;
    FKConvexElem& operator=(FKConvexElem&&) = default;
};
