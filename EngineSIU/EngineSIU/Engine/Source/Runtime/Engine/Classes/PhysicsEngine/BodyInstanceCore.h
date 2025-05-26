#pragma once
#include "UObject/ObjectMacros.h"

//class UBodySetupCore;
class UBodySetup;

struct FBodyInstanceCore
{
    DECLARE_STRUCT(FBodyInstanceCore)

public:
    FBodyInstanceCore() = default;
    virtual ~FBodyInstanceCore() = default;

    FBodyInstanceCore(const FBodyInstanceCore&) = default;
    FBodyInstanceCore& operator=(const FBodyInstanceCore&) = default;
    FBodyInstanceCore(FBodyInstanceCore&&) = default;
    FBodyInstanceCore& operator=(FBodyInstanceCore&&) = default;

public:
    //TWeakObjectPtr<UBodySetupCore> BodySetup;
    TWeakObjectPtr<UBodySetup> BodySetup;

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
