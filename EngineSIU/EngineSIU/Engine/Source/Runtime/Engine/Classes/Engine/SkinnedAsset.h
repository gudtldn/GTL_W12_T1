#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UPhysicsAsset;

class USkinnedAsset : public UObject
{
    DECLARE_CLASS(USkinnedAsset, UObject)

public:
    USkinnedAsset() = default;
    virtual ~USkinnedAsset() override = default;

    // abstract function
    virtual UPhysicsAsset* GetPhysicsAsset() const { return nullptr; }
};
