#pragma once
#include "MeshComponent.h"

class USkinnedMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

public:
    USkinnedMeshComponent() = default;
    virtual ~USkinnedMeshComponent() override = default;

    virtual void TickPose(float DeltaTime) {}

    FTransform GetBoneTransform(int32 BoneIdx) const;
    //FTransform GetBoneTransform(int32 BoneIdx, const FTransform& LocalToWorld) const;

    std::weak_ptr<USkinnedMeshComponent> LeaderPoseComponent;
};
