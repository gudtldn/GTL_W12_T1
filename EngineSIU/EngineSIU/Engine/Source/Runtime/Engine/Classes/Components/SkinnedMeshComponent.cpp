#include "SkinnedMeshComponent.h"

FTransform USkinnedMeshComponent::GetBoneTransform(int32 BoneIdx) const
{
    return FTransform();
    //return GetBoneTransform(BoneIdx, GetComponentTransform());
}

//FTransform USkinnedMeshComponent::GetBoneTransform(int32 BoneIdx, const FTransform& LocalToWorld) const
//{
//    // Handle case of use a LeaderPoseComponent - get bone matrix from there.
//    const USkinnedMeshComponent* const LeaderPoseComponentInst = LeaderPoseComponent.Get();
//    if (LeaderPoseComponentInst)
//    {
//        if (!LeaderPoseComponentInst->IsRegistered())
//        {
//            // We aren't going to get anything valid from the leader pose if it
//            // isn't valid so for now return identity
//            return FTransform::Identity;
//        }
//        if (BoneIdx < LeaderBoneMap.Num())
//        {
//            const int32 LeaderBoneIndex = LeaderBoneMap[BoneIdx];
//            const int32 NumLeaderTransforms = LeaderPoseComponentInst->GetNumComponentSpaceTransforms();
//
//            // If LeaderBoneIndex is valid, grab matrix from LeaderPoseComponent.
//            if (LeaderBoneIndex >= 0 && LeaderBoneIndex < NumLeaderTransforms)
//            {
//                return LeaderPoseComponentInst->GetComponentSpaceTransforms()[LeaderBoneIndex] * LocalToWorld;
//            }
//            else
//            {
//                // Is this a missing bone we have cached?
//                FMissingLeaderBoneCacheEntry MissingBoneInfo;
//                const FMissingLeaderBoneCacheEntry* MissingBoneInfoPtr = MissingLeaderBoneMap.Find(BoneIdx);
//                if (MissingBoneInfoPtr != nullptr)
//                {
//                    const int32 MissingLeaderBoneIndex = MissingBoneInfoPtr->CommonAncestorBoneIndex;
//                    if (MissingLeaderBoneIndex >= 0 && MissingLeaderBoneIndex < NumLeaderTransforms)
//                    {
//                        return MissingBoneInfoPtr->RelativeTransform * LeaderPoseComponentInst->GetComponentSpaceTransforms()[MissingBoneInfoPtr->CommonAncestorBoneIndex] * LocalToWorld;
//                    }
//                }
//                // Otherwise we might be able to generate the missing transform on the fly (although this is expensive)
//                else if (GetMissingLeaderBoneRelativeTransform(BoneIdx, MissingBoneInfo))
//                {
//                    const int32 MissingLeaderBoneIndex = MissingBoneInfo.CommonAncestorBoneIndex;
//                    if (MissingLeaderBoneIndex >= 0 && MissingLeaderBoneIndex < NumLeaderTransforms)
//                    {
//                        return MissingBoneInfo.RelativeTransform * LeaderPoseComponentInst->GetComponentSpaceTransforms()[MissingBoneInfo.CommonAncestorBoneIndex] * LocalToWorld;
//                    }
//                }
//
//                UE_LOG(LogSkinnedMeshComp, Display, TEXT("GetBoneTransform : ParentBoneIndex(%d) out of range of LeaderPoseComponent->SpaceBases for %s"), BoneIdx, *this->GetFName().ToString());
//                return FTransform::Identity;
//            }
//        }
//        else
//        {
//            UE_LOG(LogSkinnedMeshComp, Warning, TEXT("GetBoneTransform : BoneIndex(%d) out of range of LeaderBoneMap for %s"), BoneIdx, *this->GetFName().ToString());
//            return FTransform::Identity;
//        }
//    }
//    else
//    {
//        const int32 NumTransforms = GetNumComponentSpaceTransforms();
//        if (BoneIdx >= 0 && BoneIdx < NumTransforms)
//        {
//            return GetComponentSpaceTransforms()[BoneIdx] * LocalToWorld;
//        }
//        else
//        {
//            UE_LOG(LogSkinnedMeshComp, Display, TEXT("GetBoneTransform : BoneIndex(%d) out of range of SpaceBases for %s (%s)"), BoneIdx, *GetPathName(), GetSkinnedAsset() ? *GetSkinnedAsset()->GetFullName() : TEXT("NULL"));
//            return FTransform::Identity;
//        }
//    }
//}
