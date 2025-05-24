#pragma once
#include "UObject/ObjectMacros.h"


struct FKShapeElem
{
    DECLARE_STRUCT(FKShapeElem)

public:
    FKShapeElem() = default;
    virtual ~FKShapeElem() = default;

    FKShapeElem(const FKShapeElem&) = default;
    FKShapeElem& operator=(const FKShapeElem&) = default;
    FKShapeElem(FKShapeElem&&) = default;
    FKShapeElem& operator=(FKShapeElem&&) = default;
};
