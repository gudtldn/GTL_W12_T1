#pragma once
#include "MeshComponent.h"

class USkinnedMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

public:
    USkinnedMeshComponent() = default;
    virtual ~USkinnedMeshComponent() override = default;

    virtual void TickPose(float DeltaTime) {}

};
