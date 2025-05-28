#include "SkeletalMeshComponent.h"

#include "ReferenceSkeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Misc/FrameTime.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimTypes.h"
#include "Contents/AnimInstance/MyAnimInstance.h"
#include "Engine/Engine.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Casts.h"

#include "UObject/ObjectFactory.h"
#include "PhysicsEngine/BodySetupCore.h"

#include "PxPhysicsAPI.h"

using namespace std;
using namespace physx;

extern PxFoundation* GFoundation;
extern PxPhysics* GPhysics;
extern PxDefaultCpuDispatcher* GDispatcher;
extern PxScene* GScene;
extern PxMaterial* GMaterial;

bool USkeletalMeshComponent::bIsCPUSkinning = false;

USkeletalMeshComponent::USkeletalMeshComponent()
    : AnimationMode(EAnimationMode::AnimationSingleNode)
    , SkeletalMeshAsset(nullptr)
    , AnimClass(nullptr)
    , AnimScriptInstance(nullptr)
    , bPlayAnimation(true)
    , BonePoseContext(nullptr)
    , bRagDollSimulating(true)
{
    CPURenderData = std::make_unique<FSkeletalMeshRenderData>();
}

void USkeletalMeshComponent::InitializeComponent()
{
    Super::InitializeComponent();

    InitAnim();
}

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewComponent->SetSkeletalMeshAsset(SkeletalMeshAsset);
    NewComponent->SetAnimationMode(AnimationMode);
    if (AnimationMode == EAnimationMode::AnimationBlueprint)
    {
        NewComponent->SetAnimClass(AnimClass);
        UMyAnimInstance* AnimInstance = Cast<UMyAnimInstance>(NewComponent->GetAnimInstance());
        AnimInstance->SetPlaying(Cast<UMyAnimInstance>(AnimScriptInstance)->IsPlaying());
        // TODO: 애님 인스턴스 세팅하기
    }
    else
    {
        NewComponent->SetAnimation(GetAnimation());
    }
    NewComponent->SetLooping(this->IsLooping());
    NewComponent->SetPlaying(this->IsPlaying());
    return NewComponent;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    TickPose(DeltaTime);
}

void USkeletalMeshComponent::PhysicsTick()
{
    Super::PhysicsTick();

    if (bSimulatePhysics && Bodies.Num() > 0)
    {
        SyncBodiesToBones();
    }


    //for (auto body : Bodies)
    //{
    //    if (body->IsValidBodyInstance() && body->IsSimulatingPhysics())
    //    {
    //        body->SyncPhysXToComponent();
    //    }
    //}
}

void USkeletalMeshComponent::TickPose(float DeltaTime)
{
    if (!ShouldTickAnimation())
    {
        return;
    }

    TickAnimation(DeltaTime);
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (GetSkeletalMeshAsset())
    {
        TickAnimInstances(DeltaTime);
    }

    CPUSkinning();
}

void USkeletalMeshComponent::TickAnimInstances(float DeltaTime)
{
    if (AnimScriptInstance)
    {
        AnimScriptInstance->UpdateAnimation(DeltaTime, BonePoseContext);
    }
}

bool USkeletalMeshComponent::ShouldTickAnimation() const
{
    if (GEngine->GetWorldContextFromWorld(GetWorld())->WorldType == EWorldType::Editor)
    {
        return false;
    }
    return GetAnimInstance() && SkeletalMeshAsset && SkeletalMeshAsset->GetSkeleton();
}

bool USkeletalMeshComponent::InitializeAnimScriptInstance()
{
    USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
    
    if (NeedToSpawnAnimScriptInstance())
    {
        AnimScriptInstance = Cast<UAnimInstance>(FObjectFactory::ConstructObject(AnimClass, this));

        if (AnimScriptInstance)
        {
            AnimScriptInstance->InitializeAnimation();
        }
    }
    else
    {
        bool bShouldSpawnSingleNodeInstance = !AnimScriptInstance && SkelMesh && SkelMesh->GetSkeleton();
        if (bShouldSpawnSingleNodeInstance)
        {
            AnimScriptInstance = FObjectFactory::ConstructObject<UAnimSingleNodeInstance>(this);

            if (AnimScriptInstance)
            {
                AnimScriptInstance->InitializeAnimation();
            }
        }
    }

    return true;
}

void USkeletalMeshComponent::ClearAnimScriptInstance()
{
    if (AnimScriptInstance)
    {
        GUObjectArray.MarkRemoveObject(AnimScriptInstance);
    }
    AnimScriptInstance = nullptr;
}

void USkeletalMeshComponent::SetSkeletalMeshAsset(USkeletalMesh* InSkeletalMeshAsset)
{
    if (InSkeletalMeshAsset == GetSkeletalMeshAsset())
    {
        return;
    }
    
    SkeletalMeshAsset = InSkeletalMeshAsset;

    InitAnim();

    BonePoseContext.Pose.Empty();
    RefBonePoseTransforms.Empty();
    AABB = FBoundingBox(InSkeletalMeshAsset->GetRenderData()->BoundingBoxMin, SkeletalMeshAsset->GetRenderData()->BoundingBoxMax);
    
    if (SkeletalMeshAsset && SkeletalMeshAsset->GetSkeleton())
    {
        const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();
        BonePoseContext.Pose.InitBones(RefSkeleton.RawRefBoneInfo.Num());
        for (int32 i = 0; i < RefSkeleton.RawRefBoneInfo.Num(); ++i)
        {
            BonePoseContext.Pose[i] = RefSkeleton.RawRefBonePose[i];
            RefBonePoseTransforms.Add(RefSkeleton.RawRefBonePose[i]);
        }

        if (SkeletalMeshAsset->GetRenderData())
        {
            CPURenderData->Vertices = SkeletalMeshAsset->GetRenderData()->Vertices;
            CPURenderData->Indices = SkeletalMeshAsset->GetRenderData()->Indices;
            CPURenderData->ObjectName = SkeletalMeshAsset->GetRenderData()->ObjectName;
            CPURenderData->MaterialSubsets = SkeletalMeshAsset->GetRenderData()->MaterialSubsets;
        }

        // PhysicsAsset 처리
        UPhysicsAsset* PhysAsset = SkeletalMeshAsset->GetPhysicsAsset();
        if (!PhysAsset) // PhysicsAsset이 없다면 새로 생성
        {
            PhysAsset = FObjectFactory::ConstructObject<UPhysicsAsset>(this); 
            SkeletalMeshAsset->SetPhysicsAsset(PhysAsset);
        }
        PhysAsset->BodySetup.Empty();
        PhysAsset->ConstraintSetup.Empty();

        InitializeRagDoll(RefSkeleton);
    }
}

FTransform USkeletalMeshComponent::GetSocketTransform(FName SocketName) const
{
    FTransform Transform = FTransform::Identity;

    if (USkeleton* Skeleton = GetSkeletalMeshAsset()->GetSkeleton())
    {
        int32 BoneIndex = Skeleton->FindBoneIndex(SocketName);

        TArray<FMatrix> GlobalBoneMatrices;
        GetCurrentGlobalBoneMatrices(GlobalBoneMatrices);
        Transform = FTransform(GlobalBoneMatrices[BoneIndex]);
    }
    return Transform;
}

void USkeletalMeshComponent::GetCurrentGlobalBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const
{
    const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();
    const int32 BoneNum = RefSkeleton.RawRefBoneInfo.Num();

    OutBoneMatrices.Empty();
    OutBoneMatrices.SetNum(BoneNum);

    for (int32 BoneIndex = 0; BoneIndex < BoneNum; ++BoneIndex)
    {
        // 현재 본의 로컬 변환
        FTransform CurrentLocalTransform = BonePoseContext.Pose[BoneIndex];
        FMatrix LocalMatrix = CurrentLocalTransform.ToMatrixWithScale(); // FTransform -> FMatrix
        
        // 부모 본의 영향을 적용하여 월드 변환 구성
        int32 ParentIndex = RefSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex;
        if (ParentIndex != INDEX_NONE)
        {
            // 로컬 변환에 부모 월드 변환 적용
            LocalMatrix = LocalMatrix * OutBoneMatrices[ParentIndex];
        }
        
        // 결과 행렬 저장
        OutBoneMatrices[BoneIndex] = LocalMatrix;
    }
}

void USkeletalMeshComponent::DEBUG_SetAnimationEnabled(bool bEnable)
{
    bPlayAnimation = bEnable;
    
    if (!bPlayAnimation)
    {
        if (SkeletalMeshAsset && SkeletalMeshAsset->GetSkeleton())
        {
            const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();
            BonePoseContext.Pose.InitBones(RefSkeleton.RawRefBonePose.Num());
            for (int32 i = 0; i < RefSkeleton.RawRefBoneInfo.Num(); ++i)
            {
                BonePoseContext.Pose[i] = RefSkeleton.RawRefBonePose[i];
            }
        }
        SetElapsedTime(0.f); 
        CPURenderData->Vertices = SkeletalMeshAsset->GetRenderData()->Vertices;
        CPURenderData->Indices = SkeletalMeshAsset->GetRenderData()->Indices;
        CPURenderData->ObjectName = SkeletalMeshAsset->GetRenderData()->ObjectName;
        CPURenderData->MaterialSubsets = SkeletalMeshAsset->GetRenderData()->MaterialSubsets;
    }
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping)
{
    SetAnimation(NewAnimToPlay);
    Play(bLooping);
}

int USkeletalMeshComponent::CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const
{
    if (!AABB.Intersect(InRayOrigin, InRayDirection, OutHitDistance))
    {
        return 0;
    }
    if (SkeletalMeshAsset == nullptr)
    {
        return 0;
    }
    
    OutHitDistance = FLT_MAX;
    
    int IntersectionNum = 0;

    const FSkeletalMeshRenderData* RenderData = SkeletalMeshAsset->GetRenderData();

    const TArray<FSkeletalMeshVertex>& Vertices = RenderData->Vertices;
    const int32 VertexNum = Vertices.Num();
    if (VertexNum == 0)
    {
        return 0;
    }
    
    const TArray<UINT>& Indices = RenderData->Indices;
    const int32 IndexNum = Indices.Num();
    const bool bHasIndices = (IndexNum > 0);
    
    int32 TriangleNum = bHasIndices ? (IndexNum / 3) : (VertexNum / 3);
    for (int32 i = 0; i < TriangleNum; i++)
    {
        int32 Idx0 = i * 3;
        int32 Idx1 = i * 3 + 1;
        int32 Idx2 = i * 3 + 2;
        
        if (bHasIndices)
        {
            Idx0 = Indices[Idx0];
            Idx1 = Indices[Idx1];
            Idx2 = Indices[Idx2];
        }

        // 각 삼각형의 버텍스 위치를 FVector로 불러옵니다.
        FVector v0 = FVector(Vertices[Idx0].X, Vertices[Idx0].Y, Vertices[Idx0].Z);
        FVector v1 = FVector(Vertices[Idx1].X, Vertices[Idx1].Y, Vertices[Idx1].Z);
        FVector v2 = FVector(Vertices[Idx2].X, Vertices[Idx2].Y, Vertices[Idx2].Z);

        float HitDistance = FLT_MAX;
        if (IntersectRayTriangle(InRayOrigin, InRayDirection, v0, v1, v2, HitDistance))
        {
            OutHitDistance = FMath::Min(HitDistance, OutHitDistance);
            IntersectionNum++;
        }

    }
    return IntersectionNum;
}

const FSkeletalMeshRenderData* USkeletalMeshComponent::GetCPURenderData() const
{
    return CPURenderData.get();
}

void USkeletalMeshComponent::SetCPUSkinning(bool Flag)
{
    bIsCPUSkinning = Flag;
}

bool USkeletalMeshComponent::GetCPUSkinning()
{
    return bIsCPUSkinning;
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InAnimationMode)
{
    const bool bNeedsChange = AnimationMode != InAnimationMode;
    if (bNeedsChange)
    {
        AnimationMode = InAnimationMode;
        ClearAnimScriptInstance();
    }

    if (GetSkeletalMeshAsset() && (bNeedsChange || AnimationMode == EAnimationMode::AnimationBlueprint))
    {
        InitializeAnimScriptInstance();
    }
}

void USkeletalMeshComponent::InitAnim()
{
    if (GetSkeletalMeshAsset() == nullptr)
    {
        return;
    }

    bool bBlueprintMismatch = AnimClass && AnimScriptInstance && AnimScriptInstance->GetClass() != AnimClass;
    
    const USkeleton* AnimSkeleton = AnimScriptInstance ? AnimScriptInstance->GetCurrentSkeleton() : nullptr;
    
    const bool bClearAnimInstance = AnimScriptInstance && !AnimSkeleton;
    const bool bSkeletonMismatch = AnimSkeleton && (AnimScriptInstance->GetCurrentSkeleton() != GetSkeletalMeshAsset()->GetSkeleton());
    const bool bSkeletonsExist = AnimSkeleton && GetSkeletalMeshAsset()->GetSkeleton() && !bSkeletonMismatch;

    if (bBlueprintMismatch || bSkeletonMismatch || !bSkeletonsExist || bClearAnimInstance)
    {
        ClearAnimScriptInstance();
    }

    const bool bInitializedAnimInstance = InitializeAnimScriptInstance();

    if (bInitializedAnimInstance)
    {
        // TODO: 애니메이션 포즈 바로 반영하려면 여기에서 진행.
    }
}

bool USkeletalMeshComponent::NeedToSpawnAnimScriptInstance() const
{
    USkeletalMesh* MeshAsset = GetSkeletalMeshAsset();
    USkeleton* AnimSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
    if (AnimationMode == EAnimationMode::AnimationBlueprint && AnimClass && AnimSkeleton)
    {
        if (AnimScriptInstance == nullptr || AnimScriptInstance->GetClass() != AnimClass || AnimScriptInstance->GetOuter() != this)
        {
            return true;
        }
    }
    return false;
}

void USkeletalMeshComponent::CreatePhysicsState()
{
    // 기존 물리 상태가 있다면 먼저 파괴
    ClearPhysicsState();

    if (!SkeletalMeshAsset)
    {
        return;
    }

    if (SkeletalMeshAsset->GetPhysicsAsset() && GetWorld()) // 월드가 있어야 Scene에 추가 가능
    {
        InstantiatePhysicsAsset();
    }
}

void USkeletalMeshComponent::DestroyPhysicsState()
{
    ClearPhysicsState();
}

void USkeletalMeshComponent::ClearPhysicsState()
{
    // Constraints 먼저 해제 (Bodies를 참조할 수 있으므로)
    for (FConstraintInstance* Constraint : Constraints)
    {
        if (Constraint)
        {
            Constraint->TermConstraint(); // 내부적으로 PxJoint 해제 등
            delete Constraint;
        }
    }
    Constraints.Empty();

    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            Body->TermBody(); // 내부적으로 PxRigidActor 해제 등
            delete Body;
        }
    }
    Bodies.Empty();
}


void USkeletalMeshComponent::InstantiatePhysicsAsset()
{
    if (!(SkeletalMeshAsset && SkeletalMeshAsset->GetPhysicsAsset()))
    {
        return;
    }
}

void USkeletalMeshComponent::SyncBodiesToBones()
{
    if (!bSimulatePhysics || Bodies.Num() == 0)
    {
        return;
    }

    UPhysicsAsset* CurrentPhysicsAsset = SkeletalMeshAsset->GetPhysicsAsset();
    USkeletalMesh* CurrentSkeletalMesh = GetSkeletalMeshAsset();

    if (!CurrentPhysicsAsset || !CurrentSkeletalMesh || !CurrentSkeletalMesh->GetSkeleton())
    {
        return;
    }

    USkeleton* Skeleton = CurrentSkeletalMesh->GetSkeleton();
    const int32 NumBones = Skeleton->GetReferenceSkeleton().GetRawBoneNum();

    const FTransform ComponentToWorld = GetComponentTransform();
    const FTransform WorldToComponent = ComponentToWorld.Inverse();

    TMap<int32, FTransform> SimulatedBoneWorldTransforms;

    for (int32 BodyInstanceIndex = 0; BodyInstanceIndex < Bodies.Num(); ++BodyInstanceIndex)
    {
        FBodyInstance* BodyInst = Bodies[BodyInstanceIndex];
        if (BodyInstanceIndex >= CurrentPhysicsAsset->BodySetup.Num()) continue;
        UBodySetup* AssociatedBodySetup = CurrentPhysicsAsset->BodySetup[BodyInstanceIndex];

        if (BodyInst && BodyInst->IsValidBodyInstance() && BodyInst->IsSimulatingPhysics() && AssociatedBodySetup)
        {
            FName BoneName = AssociatedBodySetup->BoneName;
            if (BoneName == NAME_None) continue;

            int32 BoneIndex = Skeleton->FindBoneIndex(BoneName);
            if (BoneIndex == INDEX_NONE) continue;

            FTransform PhysXWorldTransform = BodyInst->GetWorldTransform();
            SimulatedBoneWorldTransforms.Add(BoneIndex, PhysXWorldTransform);
        }
    }

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        if (SimulatedBoneWorldTransforms.Contains(BoneIndex))
        {
            FTransform NewWorldTransform = SimulatedBoneWorldTransforms[BoneIndex];
            FTransform NewLocalTransform;

            int32 ParentIndex = Skeleton->GetReferenceSkeleton().GetRawRefBoneInfo()[BoneIndex].ParentIndex;

            if (ParentIndex == INDEX_NONE)
            {
                NewLocalTransform = NewWorldTransform * WorldToComponent;
            }
            else
            {
                FTransform ParentWorldTransform;
                if (SimulatedBoneWorldTransforms.Contains(ParentIndex))
                {
                    ParentWorldTransform = SimulatedBoneWorldTransforms[ParentIndex];
                }
                else
                {
                    TArray<FTransform> TempBoneSpaceTransforms;
                    TempBoneSpaceTransforms.SetNum(NumBones);
                    for (int32 i = 0; i < NumBones; ++i)
                    {
                        const FTransform& BoneLocalSpace = BonePoseContext.Pose[i];
                        int32 CurrentParentIdx = Skeleton->GetReferenceSkeleton().GetRawRefBoneInfo()[i].ParentIndex;
                        if (CurrentParentIdx == INDEX_NONE)
                        {
                            TempBoneSpaceTransforms[i] = BoneLocalSpace;
                        }
                        else
                        {
                            TempBoneSpaceTransforms[i] = BoneLocalSpace * TempBoneSpaceTransforms[CurrentParentIdx];
                        }
                    }
                    ParentWorldTransform = TempBoneSpaceTransforms[ParentIndex] * ComponentToWorld;
                }

                NewLocalTransform = NewWorldTransform.GetRelativeTransform(ParentWorldTransform);
            }

            BonePoseContext.Pose[BoneIndex] = NewLocalTransform;
        }
    }
    // MarkRenderStateDirty(); // 렌더링 상태 변경 알림 (본 트랜스폼이 바뀌었으므로)
    // UpdateBounds();       // 바운딩 볼륨 업데이트
}

void USkeletalMeshComponent::CPUSkinning(bool bForceUpdate)
{
    if (bIsCPUSkinning || bForceUpdate)
    {
         QUICK_SCOPE_CYCLE_COUNTER(SkinningPass_CPU)
         const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();
         TArray<FMatrix> CurrentGlobalBoneMatrices;
         GetCurrentGlobalBoneMatrices(CurrentGlobalBoneMatrices);
         const int32 BoneNum = RefSkeleton.RawRefBoneInfo.Num();
         
         // 최종 스키닝 행렬 계산
         TArray<FMatrix> FinalBoneMatrices;
         FinalBoneMatrices.SetNum(BoneNum);
    
         for (int32 BoneIndex = 0; BoneIndex < BoneNum; ++BoneIndex)
         {
             FinalBoneMatrices[BoneIndex] = RefSkeleton.InverseBindPoseMatrices[BoneIndex] * CurrentGlobalBoneMatrices[BoneIndex];
         }
         
         const FSkeletalMeshRenderData* RenderData = SkeletalMeshAsset->GetRenderData();
         
         for (int i = 0; i < RenderData->Vertices.Num(); i++)
         {
             FSkeletalMeshVertex Vertex = RenderData->Vertices[i];
             // 가중치 합산
             float TotalWeight = 0.0f;
    
             FVector SkinnedPosition = FVector(0.0f, 0.0f, 0.0f);
             FVector SkinnedNormal = FVector(0.0f, 0.0f, 0.0f);
             
             for (int j = 0; j < 4; ++j)
             {
                 float Weight = Vertex.BoneWeights[j];
                 TotalWeight += Weight;
     
                 if (Weight > 0.0f)
                 {
                     uint32 BoneIdx = Vertex.BoneIndices[j];
                     
                     // 본 행렬 적용 (BoneMatrices는 이미 최종 스키닝 행렬)
                     // FBX SDK에서 가져온 역바인드 포즈 행렬이 이미 포함됨
                     FVector Pos = FinalBoneMatrices[BoneIdx].TransformPosition(FVector(Vertex.X, Vertex.Y, Vertex.Z));
                     FVector4 Norm4 = FinalBoneMatrices[BoneIdx].TransformFVector4(FVector4(Vertex.NormalX, Vertex.NormalY, Vertex.NormalZ, 0.0f));
                     FVector Norm(Norm4.X, Norm4.Y, Norm4.Z);
                     
                     SkinnedPosition += Pos * Weight;
                     SkinnedNormal += Norm * Weight;
                 }
             }
    
             // 가중치 예외 처리
             if (TotalWeight < 0.001f)
             {
                 SkinnedPosition = FVector(Vertex.X, Vertex.Y, Vertex.Z);
                 SkinnedNormal = FVector(Vertex.NormalX, Vertex.NormalY, Vertex.NormalZ);
             }
             else if (FMath::Abs(TotalWeight - 1.0f) > 0.001f && TotalWeight > 0.001f)
             {
                 // 가중치 합이 1이 아닌 경우 정규화
                 SkinnedPosition /= TotalWeight;
                 SkinnedNormal /= TotalWeight;
             }
    
             CPURenderData->Vertices[i].X = SkinnedPosition.X;
             CPURenderData->Vertices[i].Y = SkinnedPosition.Y;
             CPURenderData->Vertices[i].Z = SkinnedPosition.Z;
             CPURenderData->Vertices[i].NormalX = SkinnedNormal.X;
             CPURenderData->Vertices[i].NormalY = SkinnedNormal.Y;
             CPURenderData->Vertices[i].NormalZ = SkinnedNormal.Z;
           }
     }
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
    return Cast<UAnimSingleNodeInstance>(AnimScriptInstance);
}

void USkeletalMeshComponent::SetAnimClass(UClass* NewClass)
{
    SetAnimInstanceClass(NewClass);
}

UClass* USkeletalMeshComponent::GetAnimClass()
{
    return AnimClass;
}

void USkeletalMeshComponent::SetAnimInstanceClass(class UClass* NewClass)
{
    if (NewClass != nullptr)
    {
        // set the animation mode
        const bool bWasUsingBlueprintMode = AnimationMode == EAnimationMode::AnimationBlueprint;
        AnimationMode = EAnimationMode::AnimationBlueprint;

        if (NewClass != AnimClass || !bWasUsingBlueprintMode)
        {
            // Only need to initialize if it hasn't already been set or we weren't previously using a blueprint instance
            AnimClass = NewClass;
            ClearAnimScriptInstance();
            InitAnim();
        }
    }
    else
    {
        // Need to clear the instance as well as the blueprint.
        // @todo is this it?
        AnimClass = nullptr;
        ClearAnimScriptInstance();
    }
}

void USkeletalMeshComponent::SetAnimation(UAnimationAsset* NewAnimToPlay)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetAnimationAsset(NewAnimToPlay, false);
        SingleNodeInstance->SetPlaying(false);

        // TODO: Force Update Pose and CPU Skinning
    }
}

UAnimationAsset* USkeletalMeshComponent::GetAnimation() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->GetAnimationAsset();
    }
    return nullptr;
}

void USkeletalMeshComponent::Play(bool bLooping)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPlaying(true);
        SingleNodeInstance->SetLooping(bLooping);
    }
}

void USkeletalMeshComponent::Stop()
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPlaying(false);
    }
}

void USkeletalMeshComponent::SetPlaying(bool bPlaying)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPlaying(bPlaying);
    }
}

bool USkeletalMeshComponent::IsPlaying() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->IsPlaying();
    }

    return false;
}

void USkeletalMeshComponent::SetReverse(bool bIsReverse)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetReverse(bIsReverse);
    }
}

bool USkeletalMeshComponent::IsReverse() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->IsReverse();
    }
}

void USkeletalMeshComponent::SetPlayRate(float Rate)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPlayRate(Rate);
    }
}

float USkeletalMeshComponent::GetPlayRate() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->GetPlayRate();
    }

    return 0.f;
}

void USkeletalMeshComponent::SetLooping(bool bIsLooping)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetLooping(bIsLooping);
    }
}

bool USkeletalMeshComponent::IsLooping() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->IsLooping();
    }
    return false;
}

int USkeletalMeshComponent::GetCurrentKey() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->GetCurrentKey();
    }
    return 0;
}

void USkeletalMeshComponent::SetCurrentKey(int InKey)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetCurrentKey(InKey);
    }
}

void USkeletalMeshComponent::SetElapsedTime(float InElapsedTime)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetElapsedTime(InElapsedTime);
    }
}

float USkeletalMeshComponent::GetElapsedTime() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->GetElapsedTime();
    }
    return 0.f;
}

int32 USkeletalMeshComponent::GetLoopStartFrame() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->GetLoopStartFrame();
    }
    return 0;
}

void USkeletalMeshComponent::SetLoopStartFrame(int32 InLoopStartFrame)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetLoopStartFrame(InLoopStartFrame);
    }
}

int32 USkeletalMeshComponent::GetLoopEndFrame() const
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        return SingleNodeInstance->GetLoopEndFrame();
    }
    return 0;
}

void USkeletalMeshComponent::SetLoopEndFrame(int32 InLoopEndFrame)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetLoopEndFrame(InLoopEndFrame);
    }
}
void USkeletalMeshComponent::InitializeRagDoll(const FReferenceSkeleton& InRefSkeleton)
{
    if (InRefSkeleton.GetRawBoneNum() == 0)
    {
        return;
    }
    if (!SkeletalMeshAsset)
    {
        return;
    }

    UPhysicsAsset* PhysicsAssetToPopulate = SkeletalMeshAsset->GetPhysicsAsset();
    if (!PhysicsAssetToPopulate)
    {
        return; 
    }

    //for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    //{
    //    RagdollBone& CurrentRagdollBone = RagdollBones[BoneIndex];
    //    const FMeshBoneInfo& MeshBoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex];

    //    CurrentRagdollBone.name = MeshBoneInfo.Name;
    //    CurrentRagdollBone.parentIndex = MeshBoneInfo.ParentIndex;

    //    FTransform BoneLocalTransform = InRefSkeleton.GetRawRefBonePose()[BoneIndex];
    //    CurrentRagdollBone.offset = PxVec3(BoneLocalTransform.GetLocation().X,
    //        BoneLocalTransform.GetLocation().Y,
    //        BoneLocalTransform.GetLocation().Z);

    //    // CurrentRagdollBone.halfSize = physx::PxVec3(0.f); // 필요하다면 초기화
    //}

    PopulatePhysicsAssetFromSkeleton(PhysicsAssetToPopulate, InRefSkeleton);
}

void USkeletalMeshComponent::DestroyRagDoll()
{
    RagdollBones.Empty();
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    CreateRagDollFromPhysicsAsset();

}

void USkeletalMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    DestroyPhysicsState();
    //DestroyRagDoll();
}

void USkeletalMeshComponent::PopulatePhysicsAssetFromSkeleton(UPhysicsAsset* PhysicsAssetToPopulate, const FReferenceSkeleton& InRefSkeleton)
{
    //if (!PhysicsAssetToPopulate || InRefSkeleton.GetRawBoneNum() == 0 || RagdollBones.Num() != InRefSkeleton.GetRawBoneNum())
    if (!PhysicsAssetToPopulate || InRefSkeleton.GetRawBoneNum() == 0)
    {
        return;
    }

    PhysicsAssetToPopulate->BodySetup.Empty();
    PhysicsAssetToPopulate->ConstraintSetup.Empty();

    // BodySetup 생성
    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    {
        //const RagdollBone& CurrentRagdollBone = RagdollBones[BoneIndex];
        UBodySetup* NewBodySetup = FObjectFactory::ConstructObject<UBodySetup>(PhysicsAssetToPopulate);
        NewBodySetup->BoneName = InRefSkeleton.GetRawRefBoneNames()[BoneIndex];
        NewBodySetup->bOverrideMass = true;
        NewBodySetup->Mass = DefaultBodyMass;
        NewBodySetup->LinearDamping = 0.05f;
        NewBodySetup->AngularDamping = 0.05f;

        CalculateElement(NewBodySetup, BoneIndex);

        PhysicsAssetToPopulate->BodySetup.Add(NewBodySetup);
    }

    // FIX-ME
    // ConstraintSetup 생성
    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    {
        const FMeshBoneInfo& CurrentMeshBoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex];
        if (CurrentMeshBoneInfo.ParentIndex != INDEX_NONE)
        {
            const FMeshBoneInfo& ParentMeshBoneInfo = InRefSkeleton.GetRawRefBoneInfo()[CurrentMeshBoneInfo.ParentIndex];

            UConstraintSetup* NewConstraintSetup = FObjectFactory::ConstructObject<UConstraintSetup>(PhysicsAssetToPopulate);
            NewConstraintSetup->JointName = FName(*(CurrentMeshBoneInfo.Name.ToString() + TEXT("_joint_") + ParentMeshBoneInfo.Name.ToString()));
            NewConstraintSetup->ConstraintBone1 = ParentMeshBoneInfo.Name;
            NewConstraintSetup->ConstraintBone2 = CurrentMeshBoneInfo.Name;

            const FTransform& ParentRefPose_ComponentSpace = InRefSkeleton.GetRawRefBonePose()[CurrentMeshBoneInfo.ParentIndex];
            const FTransform& ChildRefPose_ComponentSpace = InRefSkeleton.GetRawRefBonePose()[CurrentMeshBoneInfo.ParentIndex];

            const FTransform& Corrected_ChildRefPose_ComponentSpace = InRefSkeleton.GetRawRefBonePose()[BoneIndex];

            FTransform ChildLocalPose_RelativeToParent = Corrected_ChildRefPose_ComponentSpace * ParentRefPose_ComponentSpace.Inverse();

            NewConstraintSetup->LocalFrame1 = ToPxTransform(ChildLocalPose_RelativeToParent);
            NewConstraintSetup->LocalFrame2 = ToPxTransform(FTransform::Identity);

            //FTransform childJointFrameTransform;
            //childJointFrameTransform.SetRotation(FQuat(FVector::ZAxisVector, -FMath::DegreesToRadians(90.0f)));
            //NewConstraintSetup->LocalFrame2 = ToPxTransform(childJointFrameTransform);
            //FTransform parentJointFrameTransform = ChildLocalPose_RelativeToParent * childJointFrameTransform;
            //NewConstraintSetup->LocalFrame1 = ToPxTransform(parentJointFrameTransform);

            NewConstraintSetup->AngularLimits.TwistLimitAngle = 45.f;
            NewConstraintSetup->AngularLimits.Swing1LimitAngle = 30.f;
            NewConstraintSetup->AngularLimits.Swing2LimitAngle = 30.f;
            NewConstraintSetup->bDisableCollisionBetweenConstrainedBodies = true;

            PhysicsAssetToPopulate->ConstraintSetup.Add(NewConstraintSetup);
        }
    }
}


void USkeletalMeshComponent::CreateRagDollFromPhysicsAsset()
{
    if (!(SkeletalMeshAsset && SkeletalMeshAsset->GetPhysicsAsset() && SkeletalMeshAsset->GetSkeleton() && GetWorld()))
    {
        return;
    }

    UPhysicsAsset* PhysAsset = SkeletalMeshAsset->GetPhysicsAsset();
    USkeleton* CurrentSkeleton = SkeletalMeshAsset->GetSkeleton();

    if (PhysAsset->BodySetup.Num() == 0 && SkeletalMeshAsset->GetSkeleton())
    {
    }

    ClearPhysicsState(); 

    TMap<FName, FBodyInstance*> BoneNameToBodyInstanceMap;
    Bodies.Reserve(PhysAsset->BodySetup.Num());

    for (UBodySetup* Setup : PhysAsset->BodySetup)
    {
        if (Setup && Setup->BoneName != NAME_None)
        {
            FBodyInstance* NewBodyInstance = new FBodyInstance(); 
            FTransform BoneWorldTransform = FTransform::Identity;
            int32 BoneIdx = SkeletalMeshAsset->GetSkeleton()->FindBoneIndex(Setup->BoneName);
            if (BoneIdx != INDEX_NONE)
            {
                //BoneWorldTransform = GetComponentTransform() * SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton().GetRawRefBonePose()[BoneIdx];
                BoneWorldTransform = GetSocketTransform(Setup->BoneName);
            }
            else
            {
                BoneWorldTransform = GetComponentTransform(); // fallback
            }

            NewBodyInstance->InitBody(this, Setup, BoneWorldTransform, true /*bSimulatePhysics*/);
            Bodies.Add(NewBodyInstance);
            BoneNameToBodyInstanceMap.Add(Setup->BoneName, NewBodyInstance);
        }
    }
    
    // FIX-ME
    // UConstraintSetup 정보를 기반으로 FConstraintInstance 생성
    Constraints.Reserve(PhysAsset->ConstraintSetup.Num());
    for (const UConstraintSetup* CSSetup : PhysAsset->ConstraintSetup) 
    {
        if (!CSSetup) continue;

        FBodyInstance** PtrBodyInst1 = BoneNameToBodyInstanceMap.Find(CSSetup->ConstraintBone1);
        FBodyInstance** PtrBodyInst2 = BoneNameToBodyInstanceMap.Find(CSSetup->ConstraintBone2);

        if (PtrBodyInst1 && PtrBodyInst2)
        {
            FBodyInstance* BodyInst1 = *PtrBodyInst1;
            FBodyInstance* BodyInst2 = *PtrBodyInst2;

            // FBodyInstance 내부의 RigidActor가 유효한지 확인 (InitBody에서 생성되었어야 함)
            if (BodyInst1 && BodyInst2 && BodyInst1->RigidActor && BodyInst2->RigidActor)
            {
                FConstraintInstance* NewConstraintInstance = new FConstraintInstance();
                // InitConstraint 호출 시 bSimulatePhysics (또는 유사한 플래그) 전달
                NewConstraintInstance->InitConstraint(CSSetup, BodyInst1, BodyInst2, this, true);
                Constraints.Add(NewConstraintInstance);
            }
        }
    }
}
void USkeletalMeshComponent::CalculateElement(UBodySetup* InBodySetup, int32 BoneIndex)
{
    if (!InBodySetup)
    {
        // UE_LOG(LogTemp, Error, TEXT("CalculateElement: InBodySetup is null for BoneIndex %d"), BoneIndex);
        return;
    }

    const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();

    if (BoneIndex < 0 || BoneIndex >= RefSkeleton.GetRawBoneNum())
    {
        // UE_LOG(LogTemp, Error, TEXT("CalculateElement: Invalid BoneIndex %d. RagdollBones.Num() is %d"), BoneIndex, RagdollBones.Num());
        return;
    }

    FKSphylElem CapsuleElem;

    // 1. 고정된 반지름 설정
    const float FixedRadius = 0.9f;
    CapsuleElem.Radius = FixedRadius;

    // 2. 캡슐 길이(원통 부분) 계산: 현재 본에서 자식 본까지의 거리
    float CalculatedCylinderLength = 0.0f;
    FVector BoneDirection = FVector::XAxisVector; // 기본 방향 (캡슐의 길이 방향 축)
    FVector ChildLocalPos = FVector::XAxisVector;
    bool bHasChild = false;

    for (int32 ChildSearchIdx = 0; ChildSearchIdx < RefSkeleton.GetRawBoneNum(); ++ChildSearchIdx)
    {
        // RagdollBones[ChildSearchIdx].parentIndex가 현재 BoneIndex와 같은 자식 본을 찾음
        if (RefSkeleton.GetParentIndex(ChildSearchIdx) == BoneIndex)
        {
            
            // 자식 본의 부모 기준 오프셋 벡터 (RagdollBones[ChildSearchIdx].offset은 부모(현재 BoneIndex) 기준 자식의 로컬 위치)
            //ChildLocalPos = FVector(RagdollBones[ChildSearchIdx].offset.x,
            //    RagdollBones[ChildSearchIdx].offset.y,
            //    RagdollBones[ChildSearchIdx].offset.z);

            // FIX-ME
            ChildLocalPos = RefSkeleton.GetRawRefBonePose()[ChildSearchIdx].GetLocation();

            CalculatedCylinderLength = ChildLocalPos.Size() * 0.8f; // 자식까지의 거리를 원통 길이로 사용

            if (CalculatedCylinderLength > KINDA_SMALL_NUMBER)
            {
                BoneDirection = ChildLocalPos.GetSafeNormal(); // 본의 방향 설정
            }
            else // 자식과의 거리가 매우 짧은 경우
            {
                CalculatedCylinderLength = MinCylinderLength; // 최소 길이 사용
                // BoneDirection은 기본값(XAxisVector) 유지 또는 다른 방식 사용
            }
            bHasChild = true;
            break; // 첫 번째 자식만 사용하여 길이와 방향 결정
        }
    }

    if (!bHasChild)
    {
        CalculatedCylinderLength = MinCylinderLength; 
    }

    if (CalculatedCylinderLength < MinCylinderLength)
    {
        CalculatedCylinderLength = MinCylinderLength;
    }

    CapsuleElem.Length = CalculatedCylinderLength;

    CapsuleElem.Center = ChildLocalPos / 2;

    if (BoneDirection.IsNearlyZero() || BoneDirection.Equals(FVector::XAxisVector))
    {
        CapsuleElem.Rotation = FRotator::ZeroRotator;
    }
    else
    {
        FQuat RotQuat = FQuat::FindBetween(FVector::XAxisVector, BoneDirection);
        CapsuleElem.Rotation = RotQuat.Rotator();
    }

    if (BoneIndex == 0) return;
    InBodySetup->AggGeom.SphylElems.Add(CapsuleElem);

}


PxTransform USkeletalMeshComponent::ToPxTransform(const FTransform& UnrealTransform)
{
    const FVector Position = UnrealTransform.GetLocation();
    FQuat Quaternion = UnrealTransform.GetRotation();
    Quaternion.Normalize();
    return PxTransform(
        PxVec3(Position.X, Position.Y, Position.Z),
        PxQuat(Quaternion.X, Quaternion.Z, Quaternion.Z, Quaternion.W)
    );
}
