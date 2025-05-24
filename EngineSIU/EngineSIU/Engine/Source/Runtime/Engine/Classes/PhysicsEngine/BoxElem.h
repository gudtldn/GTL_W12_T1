#pragma once
#include "ShapeElem.h"
#include "UObject/ObjectMacros.h"


struct FKBoxElem : public FKShapeElem
{
    DECLARE_STRUCT(FKBoxElem, FKShapeElem)

public:
    FKBoxElem() = default;
    virtual ~FKBoxElem() override = default;

    FKBoxElem(const FKBoxElem&) = default;
    FKBoxElem& operator=(const FKBoxElem&) = default;
    FKBoxElem(FKBoxElem&&) = default;
    FKBoxElem& operator=(FKBoxElem&&) = default;
};
