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

    void SetRelativeTransform(const FTransform& InTransform);
    FTransform GetRelativeTransform() const;
    
protected:
    /** 부모 컴포넌트로부터 상대적인 위치 */
    UPROPERTY
    (FVector, RelativeLocation)

    /** 부모 컴포넌트로부터 상대적인 회전 */
    UPROPERTY
    (FRotator, RelativeRotation)

    /** 부모 컴포넌트로부터 상대적인 크기 */
    UPROPERTY
    (FVector, RelativeScale3D)

    UPROPERTY
    (FTransform, Transform)
};
