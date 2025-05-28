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

#include <iostream>
#include <codecvt> 
#include <string>  
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
    if (BodyInstance.IsValidBodyInstance() && BodyInstance.IsSimulatingPhysics())
    {
        BodyInstance.SyncPhysXToComponent();
    }

    if (bSimulatePhysics && Bodies.Num() > 0)
    {
        SyncBodiesToBones();
        UpdateRagdoll();
    }
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
    
    const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();
    BonePoseContext.Pose.InitBones(RefSkeleton.RawRefBoneInfo.Num());
    for (int32 i = 0; i < RefSkeleton.RawRefBoneInfo.Num(); ++i)
    {
        BonePoseContext.Pose[i] = RefSkeleton.RawRefBonePose[i];
        RefBonePoseTransforms.Add(RefSkeleton.RawRefBonePose[i]);
    }
    
    CPURenderData->Vertices = InSkeletalMeshAsset->GetRenderData()->Vertices;
    CPURenderData->Indices = InSkeletalMeshAsset->GetRenderData()->Indices;
    CPURenderData->ObjectName = InSkeletalMeshAsset->GetRenderData()->ObjectName;
    CPURenderData->MaterialSubsets = InSkeletalMeshAsset->GetRenderData()->MaterialSubsets;

    UPhysicsAsset* PhysAsset = new UPhysicsAsset();
    SkeletalMeshAsset->SetPhysicsAsset(PhysAsset);
    PhysAsset->BodySetup.Empty();
    PhysAsset->ConstraintSetup.Empty();

    InitializeRagDoll(RefSkeleton);
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

    UPhysicsAsset* PhysAsset = SkeletalMeshAsset->GetPhysicsAsset();
    const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();

    PopulatePhysicsAssetFromSkeleton(PhysAsset, RefSkeleton);
}

void USkeletalMeshComponent::SyncBodiesToBones()
{
    // 이 함수는 물리 시뮬레이션이 완료된 후 (FPhysX::Tick 이후) 호출되어야 합니다.
    // 그리고 bSimulatePhysics가 true일 때만 의미가 있습니다.

    if (!bSimulatePhysics || Bodies.Num() == 0)
    {
        return;
    }

    UPhysicsAsset* CurrentPhysicsAsset = SkeletalMeshAsset->GetPhysicsAsset();
    USkeletalMesh* CurrentSkeletalMesh = GetSkeletalMeshAsset(); // 또는 멤버 변수 직접 사용

    if (!CurrentPhysicsAsset || !CurrentSkeletalMesh || !CurrentSkeletalMesh->GetSkeleton())
    {
        // 필요한 에셋 정보가 없으면 동기화 불가
        // UE_LOG(LogSkeletalMesh, Warning, TEXT("SyncBodiesToBones: Missing PhysicsAsset, SkeletalMesh, or Skeleton."));
        return;
    }

    USkeleton* Skeleton = CurrentSkeletalMesh->GetSkeleton();

    // 컴포넌트의 현재 월드 트랜스폼 (본 트랜스폼을 컴포넌트 공간으로 변환 시 필요)
    // const FTransform ComponentToWorldInverse = GetComponentToWorld().Inverse();

    for (int32 BodyInstanceIndex = 0; BodyInstanceIndex < Bodies.Num(); ++BodyInstanceIndex)
    {
        FBodyInstance* BodyInst = Bodies[BodyInstanceIndex];

        // 해당 FBodyInstance에 대한 UBodySetup 정보 가져오기
        // (PhysicsAsset의 BodySetup 배열과 Bodies 배열의 인덱스가 일치한다고 가정)
        if (BodyInstanceIndex >= CurrentPhysicsAsset->BodySetup.Num())
        {
            // UE_LOG(LogSkeletalMesh, Warning, TEXT("SyncBodiesToBones: BodyInstanceIndex out of bounds for PhysicsAsset BodySetups."));
            continue;
        }
        UBodySetup* AssociatedBodySetup = CurrentPhysicsAsset->BodySetup[BodyInstanceIndex];

        if (BodyInst && BodyInst->IsValidBodyInstance() && BodyInst->IsSimulatingPhysics() && AssociatedBodySetup)
        {
            FName BoneName = AssociatedBodySetup->BoneName; // UBodySetup에서 본 이름 가져오기
            if (BoneName == NAME_None)
            {
                // 이 BodySetup이 특정 본에 연결되지 않았다면 (예: 전체를 감싸는 하나의 바디),
                // 컴포넌트 자체의 트랜스폼을 업데이트하거나 다른 처리를 할 수 있음.
                // 여기서는 본에 연결된 경우만 처리.
                continue;
            }

            // 본 인덱스 확인
            int32 BoneIndex = Skeleton->FindBoneIndex(BoneName);
            if (BoneIndex == INDEX_NONE)
            {
                // UE_LOG(LogSkeletalMesh, Warning, TEXT("SyncBodiesToBones: Bone '%s' not found in Skeleton."), *BoneName.ToString());
                continue;
            }

            // 1. FBodyInstance로부터 PhysX Actor의 현재 월드 트랜스폼을 가져옵니다.
            FTransform PhysXWorldTransform = BodyInst->GetWorldTransform();

            // 2. 가져온 월드 트랜스폼을 해당 본에 직접 설정합니다.
            //    이 작업은 애니메이션 결과를 덮어쓰게 됩니다.
            //    SetBoneWorldSpaceTransform 함수가 있다고 가정.
            //    이 함수는 내부적으로 다른 자식 본들의 상대적 위치도 업데이트해야 할 수 있음 (Forward Kinematics).
            //    또는, 단순히 해당 본의 공간 변환 정보만 업데이트하고,
            //    메시 디포메이션은 나중에 전체 본 계층 구조를 기반으로 수행될 수 있음.

            // 예시: 가상의 SetBoneWorldTransform 함수 호출
            // SetBoneWorldTransform(BoneIndex, PhysXWorldTransform);
            // 또는
            // SetBoneTransformByName(BoneName, PhysXWorldTransform, EBoneSpaces::WorldSpace);

            // 만약 본 트랜스폼을 컴포넌트 공간으로 설정해야 한다면:
            // FTransform BoneComponentSpaceTransform = PhysXWorldTransform * ComponentToWorldInverse;
            // SetBoneTransformByName(BoneName, BoneComponentSpaceTransform, EBoneSpaces::ComponentSpace);

            // UE_LOG(LogSkeletalMesh, Verbose, TEXT("Bone '%s' (Index %d) synced to PhysX Transform: %s"),
            //     *BoneName.ToString(), BoneIndex, *PhysXWorldTransform.ToString());


            // 실제 언리얼 엔진에서는 이 과정이 더 복잡합니다.
            // - SpaceBases (로컬 공간), BoneSpaceTransforms (컴포넌트 공간) 등을 업데이트.
            // - Kinematic 본과 Simulated 본을 구분하여 처리.
            // - 텔레포트 여부, 물리 블렌딩 가중치 등을 고려.
            // - `FillComponentSpaceTransforms()` 같은 함수를 통해 최종 컴포넌트 공간 변환 배열을 채움.

            // 여기서는 가장 기본적인 "물리 결과를 본의 월드 트랜스폼으로 직접 적용"하는 로직을 가정합니다.
            // 사용자 엔진의 스켈레탈 애니메이션 시스템 구조에 맞춰 구체적인 본 트랜스폼 설정 방식을 구현해야 합니다.
            // 예를 들어, 각 본의 FMatrix 또는 FTransform 배열을 직접 업데이트할 수 있습니다.
            // GetEditableBoneTransform(BoneIndex).SetFromMatrix(PhysXWorldTransform.ToMatrixWithScale());
        }
    }

    // 모든 물리 본의 트랜스폼이 업데이트된 후,
    // 컴포넌트의 최종 바운딩 박스 등을 다시 계산해야 할 수 있습니다.
    // MarkRenderTransformDirty(); // 렌더링 시스템에 트랜스폼 변경 알림
    // UpdateBounds(); // 바운딩 볼륨 업데이트
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

    UPhysicsAsset* PhysicsAsset = SkeletalMeshAsset->GetPhysicsAsset();

    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); BoneIndex++)
    {
        FName Name = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex].Name;
        int ParentIndex = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex].ParentIndex;

        FTransform ParentBoneTransform = FTransform::Identity;
        if (ParentIndex != -1)
        {
            //ParentBoneTransform = InRefSkeleton.GetRawRefBonePose()[ParentIndex].GetRelativeTransform(FTransform::Identity);
            ParentBoneTransform = InRefSkeleton.GetRawRefBonePose()[ParentIndex];
        }

        FTransform CurrentTransform = InRefSkeleton.GetRawRefBonePose()[BoneIndex].GetRelativeTransform(ParentBoneTransform);
        PxVec3 Offset = PxVec3(CurrentTransform.GetLocation().X, CurrentTransform.GetLocation().Y, CurrentTransform.GetLocation().Z);
        PxVec3 HalfSize = PxVec3(CurrentTransform.GetScale3D().X, CurrentTransform.GetScale3D().Y, CurrentTransform.GetScale3D().Z);

        RagdollBones.Add({ Name, Offset, HalfSize, ParentIndex });

        ///// Begin Test
        UBodySetup* NewBodySetup = FObjectFactory::ConstructObject<UBodySetup>(PhysicsAsset);
        NewBodySetup->AggGeom.SphylElems.AddDefaulted();

        CalculateElement(NewBodySetup, InRefSkeleton, BoneIndex);

        PhysicsAsset->BodySetup.Add(NewBodySetup);

    }
}

//void USkeletalMeshComponent::CreateRagDoll(const PxVec3& WorldRoot)
//{
//    for (int i = 0; i < RagdollBones.Num(); ++i)
//    {
//        RagdollBone& bone = RagdollBones[i];
//
//        // 부모의 위치 기준으로 위치 계산
//        PxVec3 parentPos = (bone.parentIndex >= 0) ? RagdollBones[bone.parentIndex].body->getGlobalPose().p : WorldRoot;
//        PxVec3 bonePos = parentPos + bone.offset;
//
//        // 바디 생성
//        PxTransform pose(bonePos);
//        PxRigidDynamic* body = GPhysics->createRigidDynamic(pose);
//        PxShape* shape = GPhysics->createShape(PxCapsuleGeometry(bone.halfSize.x, bone.halfSize.y), *GMaterial);
//        body->attachShape(*shape);
//        PxRigidBodyExt::updateMassAndInertia(*body, 1.0f);
//        GScene->addActor(*body);
//        bone.body = body;
//
//        // 조인트 연결
//        if (bone.parentIndex >= 0)
//        {
//            RagdollBone& parent = RagdollBones[bone.parentIndex];
//
//            PxTransform localFrameParent = PxTransform(parent.body->getGlobalPose().getInverse() * PxTransform(bonePos));
//            PxTransform localFrameChild = PxTransform(PxVec3(0));
//
//            PxD6Joint* joint = PxD6JointCreate(*GPhysics, parent.body, localFrameParent, bone.body, localFrameChild);
//
//            // 각도 제한 설정
//            joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
//            joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
//            joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
//            joint->setTwistLimit(PxJointAngularLimitPair(-PxPi / 4, PxPi / 4));
//            joint->setSwingLimit(PxJointLimitCone(PxPi / 6, PxPi / 6));
//
//            bone.joint = joint;
//        }
//    }
//}

void USkeletalMeshComponent::DestroyRagDoll()
{

}

void USkeletalMeshComponent::UpdateRagdoll()
{
    if (bRagDollSimulating)
    {
        //for (auto Body : Bodies)
        //{
        //    FVector Location = Body->GetWorldTransform().GetLocation();
        //    PxTransform t = { Location.X, Location.Y, Location.Z };
        //    PxMat44 m(t);
        //}
        //for (auto Constraint : Constraints)
        //{
        //    //FVector Location = Constraint->GetWorldTransform().GetLocation();
        //    //PxTransform t = { Location.X, Location.Y, Location.Z };
        //    //PxMat44 m(t);
        //}

        UPhysicsAsset* PhysAsset = SkeletalMeshAsset->GetPhysicsAsset();
        const FReferenceSkeleton& RefSkeleton = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton();

        PopulatePhysicsAssetFromSkeleton(PhysAsset, RefSkeleton);
    }
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    CreatePhysicsState();
    CreateRagDollFromPhysicsAsset();

}

void USkeletalMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    DestroyPhysicsState();
    DestroyRagDoll();
}

void USkeletalMeshComponent::PopulatePhysicsAssetFromSkeleton(UPhysicsAsset* PhysicsAssetToPopulate, const FReferenceSkeleton& InRefSkeleton)
{
    if (!PhysicsAssetToPopulate || InRefSkeleton.GetRawBoneNum() == 0)
    {
        return;
    }

    // Begin Body
    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    {
        const FMeshBoneInfo& BoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex];

        // UBodySetup* NewBodySetup = FObjectFactory::ConstructObject<UBodySetup>(PhysicsAssetToPopulate);
        UBodySetup* NewBodySetup = FObjectFactory::ConstructObject<UBodySetup>(PhysicsAssetToPopulate, BoneInfo.Name);
        NewBodySetup->BoneName = BoneInfo.Name;
        NewBodySetup->bOverrideMass = true;
        NewBodySetup->Mass = DefaultBodyMass;
        NewBodySetup->LinearDamping = 0.05f;
        NewBodySetup->AngularDamping = 0.05f;

        CalculateElement(NewBodySetup, InRefSkeleton, BoneIndex);
        PhysicsAssetToPopulate->BodySetup.Add(NewBodySetup);
    }
    // End Body
     
    //// FIX-ME
    // Begin Constraint
    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    {
        const FMeshBoneInfo& BoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex];
        if (BoneInfo.ParentIndex != INDEX_NONE) // 루트 본이 아니면 부모와 조인트 생성
        {
            const FMeshBoneInfo& ParentBoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneInfo.ParentIndex];

            //UConstraintSetup* NewConstraintSetup = FObjectFactory::ConstructObject<UConstraintSetup>(PhysicsAssetToPopulate);
            UConstraintSetup* NewConstraintSetup = FObjectFactory::ConstructObject<UConstraintSetup>(PhysicsAssetToPopulate, FName(*(BoneInfo.Name.ToString() + TEXT("_joint"))));
            NewConstraintSetup->JointName = FName(*(BoneInfo.Name.ToString() + TEXT("_joint_") + ParentBoneInfo.Name.ToString())); // 좀 더 상세한 이름
            NewConstraintSetup->ConstraintBone1 = ParentBoneInfo.Name; // 부모 본
            NewConstraintSetup->ConstraintBone2 = BoneInfo.Name;   // 자식 본
 
            const FTransform& ParentRefPose_ComponentSpace = InRefSkeleton.GetRawRefBonePose()[BoneInfo.ParentIndex];
            const FTransform& ChildRefPose_ComponentSpace = InRefSkeleton.GetRawRefBonePose()[BoneIndex];
            NewConstraintSetup->LocalFrame1 = ChildRefPose_ComponentSpace * ParentRefPose_ComponentSpace.Inverse();
            NewConstraintSetup->LocalFrame2 = FTransform::Identity;

            NewConstraintSetup->AngularLimits.TwistLimitAngle = 45.f;
            NewConstraintSetup->AngularLimits.Swing1LimitAngle = 30.f;
            NewConstraintSetup->AngularLimits.Swing2LimitAngle = 30.f;
            NewConstraintSetup->bDisableCollisionBetweenConstrainedBodies = true; // 기본적으로 연결된 바디끼리 충돌 안함

            PhysicsAssetToPopulate->ConstraintSetup.Add(NewConstraintSetup);
        }
    }
    // End Constraint
}

void USkeletalMeshComponent::CreateRagDollFromPhysicsAsset()
{
    if (!(SkeletalMeshAsset && SkeletalMeshAsset->GetPhysicsAsset() && SkeletalMeshAsset->GetSkeleton() && GetWorld()))
    {
        return;
    }

    UPhysicsAsset* PhysAsset = SkeletalMeshAsset->GetPhysicsAsset();
    USkeleton* CurrentSkeleton = SkeletalMeshAsset->GetSkeleton();

    ClearPhysicsState();

    TMap<FName, FBodyInstance*> BoneNameToBodyInstanceMap;
    Bodies.Reserve(PhysAsset->BodySetup.Num());

    for (UBodySetup* Setup : PhysAsset->BodySetup)
    {
        if (Setup && Setup->BoneName != NAME_None)
        {
            FBodyInstance* NewBodyInstance = new FBodyInstance();

            FTransform BoneRefPoseGlobalTransform = FTransform::Identity; // 계산 필요
            int32 BoneIdx = CurrentSkeleton->FindBoneIndex(Setup->BoneName);
            if (BoneIdx != INDEX_NONE)
            {
                BoneRefPoseGlobalTransform = GetComponentTransform() * CurrentSkeleton->GetReferenceSkeleton().GetRawRefBonePose()[BoneIdx];
            }

            NewBodyInstance->InitBody(this, Setup, BoneRefPoseGlobalTransform, true /*bSimulatePhysics*/);
            Bodies.Add(NewBodyInstance);
            BoneNameToBodyInstanceMap.Add(Setup->BoneName, NewBodyInstance);
        }
    }

    //// FIX-ME
    // Begin Constraint
    for (const auto CSSetup : PhysAsset->ConstraintSetup)
    {
        FBodyInstance** PtrBodyInst1 = BoneNameToBodyInstanceMap.Find(CSSetup->ConstraintBone1);
        FBodyInstance** PtrBodyInst2 = BoneNameToBodyInstanceMap.Find(CSSetup->ConstraintBone2);

        if (PtrBodyInst1 && PtrBodyInst2)
        {
            FBodyInstance* BodyInst1 = *PtrBodyInst1;
            FBodyInstance* BodyInst2 = *PtrBodyInst2;

            if (BodyInst1 && BodyInst2 && BodyInst1->RigidActor && BodyInst2->RigidActor)
            {
                FConstraintInstance* NewConstraintInstance = new FConstraintInstance();
                NewConstraintInstance->InitConstraint(CSSetup, BodyInst1, BodyInst2, this, true);
                Constraints.Add(NewConstraintInstance);
            }
        }
    }
    // End Constraint
}

void USkeletalMeshComponent::CalculateElement(UBodySetup* InBodySetup, const FReferenceSkeleton& InRefSkeleton, int32 BoneIndex)
{
    FKSphylElem CapsuleElem;
    CapsuleElem.Center = InRefSkeleton.GetRawRefBonePose()[BoneIndex].GetLocation(); // 본의 로컬 원점을 기준으로 콜리전 위치 지정

    float EstimatedBoneLength = -1.0f;
    FVector ChildDirection = FVector::ForwardVector; // 기본 방향 (예: X축)

    for (int32 ChildSearchIdx = 0; ChildSearchIdx < InRefSkeleton.GetRawBoneNum(); ++ChildSearchIdx)
    {
        if (InRefSkeleton.GetRawRefBoneInfo()[ChildSearchIdx].ParentIndex == BoneIndex)
        {
            FVector ChildLocalPos = InRefSkeleton.GetRawRefBonePose()[ChildSearchIdx].GetLocation();
            EstimatedBoneLength = ChildLocalPos.Size();
            if (EstimatedBoneLength > KINDA_SMALL_NUMBER)
            {
                ChildDirection = ChildLocalPos.GetSafeNormal();
            }
            break;
        }
    }

    if (EstimatedBoneLength <= KINDA_SMALL_NUMBER)
    {
        EstimatedBoneLength = DefaultBoneLength;
    }

    CalculatedRadius = FMath::Max(MinRadius, EstimatedBoneLength * 0.25f);

    CalculatedCylinderLength = EstimatedBoneLength - (2.0f * CalculatedRadius);

    if (CalculatedCylinderLength < MinCylinderLength)
    {
        CalculatedRadius = FMath::Max(MinRadius, (EstimatedBoneLength - MinCylinderLength) / 2.0f);
        CalculatedCylinderLength = FMath::Max(MinCylinderLength, EstimatedBoneLength - (2.0f * CalculatedRadius));

        if (CalculatedCylinderLength < MinCylinderLength || CalculatedRadius < MinRadius)
        {
            CalculatedRadius = MinRadius;
            CalculatedCylinderLength = MinCylinderLength;
        }
    }

    CapsuleElem.Radius = FMath::Clamp(CalculatedRadius, MinRadius, MaxRadius);
    CapsuleElem.Length = CalculatedCylinderLength;

    if (ChildDirection.IsNearlyZero() || ChildDirection.Equals(FVector::ForwardVector))
    {
        CapsuleElem.Rotation = FRotator::ZeroRotator;
    }
    else
    {
        FQuat RotQuat = FQuat::FindBetween(FVector::ForwardVector, ChildDirection);
        CapsuleElem.Rotation = RotQuat.Rotator();
    }

    InBodySetup->AggGeom.SphylElems.Add(CapsuleElem);
}
