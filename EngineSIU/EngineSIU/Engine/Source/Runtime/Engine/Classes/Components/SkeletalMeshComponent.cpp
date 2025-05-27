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

bool USkeletalMeshComponent::bIsCPUSkinning = false;

USkeletalMeshComponent::USkeletalMeshComponent()
    : AnimationMode(EAnimationMode::AnimationSingleNode)
    , SkeletalMeshAsset(nullptr)
    , AnimClass(nullptr)
    , AnimScriptInstance(nullptr)
    , bPlayAnimation(true)
    ,BonePoseContext(nullptr)
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
    if (bSimulatePhysics && Bodies.Num() > 0)
    {
        SyncBodiesToBones();
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

    // 1. Bodies 생성
    Bodies.Reserve(PhysAsset->BodySetup.Num());
    for (UBodySetup* Setup : PhysAsset->BodySetup)
    {
        if (Setup)
        {
            FBodyInstance* NewBodyInstance = new FBodyInstance();

            // Setup->BoneName을 사용하여 이 BodySetup에 연결된 본의 인덱스를 찾고,
            // 해당 본의 현재 월드 또는 컴포넌트 공간 트랜스폼을 가져옵니다.
            // 이 부분은 스켈레탈 애니메이션 시스템과의 연동이 필요합니다.
            FTransform BodyTransform = GetComponentTransform(); // 기본값: 컴포넌트 트랜스폼
            int32 BoneIndex = SkeletalMeshAsset->GetSkeleton()->FindBoneIndex(Setup->BoneName);
            if (BoneIndex != INDEX_NONE)
            {
                // BodyTransform = GetBoneTransform(BoneIndex); // 컴포넌트 공간 또는 월드 공간 트랜스폼 가져오기
            }

            // InitBody 호출. UPrimitiveComponent 대신 this (USkeletalMeshComponent*)를 전달하거나,
            // FBodyInstance가 UActorComponent*를 받도록 수정할 수 있습니다.
            // 여기서는 UPrimitiveComponent의 파생 클래스라고 가정하고 this 전달.
            NewBodyInstance->InitBody(this, Setup, BodyTransform, bSimulatePhysics); // bSimulatePhysics는 USMC 멤버
            Bodies.Add(NewBodyInstance);
        }
    }

    // 2. Constraints 생성 (UPhysicsAsset에 ConstraintSetup 배열이 있다고 가정)
    // if (PhysAsset->ConstraintSetups.Num() > 0) // ConstraintSetups는 UPhysicsAsset의 TArray<UConstraintSetup*> 멤버라고 가정
    // {
    //     Constraints.Reserve(PhysAsset->ConstraintSetups.Num());
    //     for (UConstraintSetup* ConstraintSetup : PhysAsset->ConstraintSetups)
    //     {
    //         if (ConstraintSetup)
    //         {
    //             // ConstraintSetup에서 필요한 정보 (연결할 두 Body의 인덱스 또는 이름, 조인트 설정 등)를 가져옵니다.
    //             // 해당 Body 인덱스를 사용하여 Bodies 배열에서 FBodyInstance 포인터를 찾습니다.
    //             FBodyInstance* Body1 = nullptr;
    //             FBodyInstance* Body2 = nullptr;
    //             // ... (ConstraintSetup->BoneName1, ConstraintSetup->BoneName2 등으로 Bodies 배열에서 검색) ...
    //
    //             if (Body1 && Body2) // 두 Body가 모두 유효해야 조인트 생성 가능
    //             {
    //                 FConstraintInstance* NewConstraintInstance = new FConstraintInstance();
    //                 // NewConstraintInstance->InitConstraint(this, ConstraintSetup, Body1, Body2);
    //                 Constraints.Add(NewConstraintInstance);
    //             }
    //         }
    //     }
    // }
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
