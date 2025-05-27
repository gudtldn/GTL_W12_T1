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
    // 하드코딩된 값으로 채워야 함.


    // Begin Test
    //if (bRagDollSimulating)
    //{
    //    InitializeRagDoll(RefSkeleton);
    //}
    InitializeRagDoll(RefSkeleton);
    // End Test
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
    for (UConstraintInstance* Constraint : Constraints)
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

    /*

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
                // 컴포넌트 공간 또는 월드 공간 트랜스폼 가져오기
                BodyTransform = SkeletalMeshAsset->GetSkeleton()->GetReferenceSkeleton().GetRawRefBonePose()[BoneIndex]; 
            }

            NewBodyInstance->InitBody(this, Setup, BodyTransform, bSimulatePhysics);
            Bodies.Add(NewBodyInstance);
        }
    }

    // 2. Constraints 생성 (UPhysicsAsset에 ConstraintSetup 배열이 있다고 가정)
    if (PhysAsset->ConstraintSetup.Num() > 0) // ConstraintSetups는 UPhysicsAsset의 TArray<UConstraintSetup*> 멤버라고 가정
    {
        Constraints.Reserve(PhysAsset->ConstraintSetup.Num());
        for (UConstraintSetup* Setup : PhysAsset->ConstraintSetup)
        {
            if (Setup)
            {
                // ConstraintSetup에서 필요한 정보 (연결할 두 Body의 인덱스 또는 이름, 조인트 설정 등)를 가져옵니다.
                // 해당 Body 인덱스를 사용하여 Bodies 배열에서 FBodyInstance 포인터를 찾습니다.
                FBodyInstance* Body1 = nullptr;
                FBodyInstance* Body2 = nullptr;

                if (Setup->ConstraintBone1 != NAME_None && Setup->ConstraintBone2 != NAME_None)
                {
                    // ConstraintBone1과 ConstraintBone2는 UBodySetup의 BoneName과 일치하는 이름입니다.
                    int32 BoneIndex1 = SkeletalMeshAsset->GetSkeleton()->FindBoneIndex(Setup->ConstraintBone1);
                    int32 BoneIndex2 = SkeletalMeshAsset->GetSkeleton()->FindBoneIndex(Setup->ConstraintBone2);
                    if (BoneIndex1 != INDEX_NONE && BoneIndex2 != INDEX_NONE)
                    {
                        // Bodies 배열에서 해당 본 인덱스에 해당하는 FBodyInstance를 찾습니다.
                        Body1 = Bodies[BoneIndex1];
                        Body2 = Bodies[BoneIndex2];
                    }
                    Setup->ConstraintBone1 = RefSkeleton.RawRefBoneInfo[BoneIndex1].Name;
                    Setup->ConstraintBone2 = RefSkeleton.RawRefBoneInfo[BoneIndex2].Name;
                }
                
                if (Body1 && Body2) // 두 Body가 모두 유효해야 조인트 생성 가능
                {
                    FConstraintInstance* NewConstraintInstance = new FConstraintInstance();
                    NewConstraintInstance->InitConstraint(Setup, Body1, Body2, this, true);
                    Constraints.Add(NewConstraintInstance);
                }
            }
        }
    }
    */
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
    PhysicsAsset->BodySetup.Empty();

    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); BoneIndex++)
    {
        FName Name = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex].Name;
        int ParentIndex = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex].ParentIndex;

        FTransform ParentBoneTransform = FTransform::Identity;
        if (ParentIndex != -1)
        {
            ParentBoneTransform = InRefSkeleton.GetRawRefBonePose()[ParentIndex].GetRelativeTransform(FTransform::Identity);
        }

        FTransform CurrentTransform = InRefSkeleton.GetRawRefBonePose()[BoneIndex].GetRelativeTransform(ParentBoneTransform);
        PxVec3 Offset = PxVec3(CurrentTransform.GetLocation().X, CurrentTransform.GetLocation().Y, CurrentTransform.GetLocation().Z);
        PxVec3 HalfSize = PxVec3(CurrentTransform.GetScale3D().X, CurrentTransform.GetScale3D().Y, CurrentTransform.GetScale3D().Z);

        RagdollBones.Add({ Name, Offset, HalfSize, ParentIndex });

        ///// Begin Test
        UBodySetup* NewBodySetup = FObjectFactory::ConstructObject<UBodySetup>(PhysicsAsset);
        NewBodySetup->AggGeom.SphylElems.AddDefaulted();
        PhysicsAsset->BodySetup.Add(NewBodySetup);
    }
}

void USkeletalMeshComponent::CreateRagDoll(const PxVec3& WorldRoot)
{
    for (int i = 0; i < RagdollBones.Num(); ++i)
    {
        RagdollBone& bone = RagdollBones[i];

        // 부모의 위치 기준으로 위치 계산
        PxVec3 parentPos = (bone.parentIndex >= 0) ? RagdollBones[bone.parentIndex].body->getGlobalPose().p : WorldRoot;
        PxVec3 bonePos = parentPos + bone.offset;

        // 바디 생성
        PxTransform pose(bonePos);
        PxRigidDynamic* body = GPhysics->createRigidDynamic(pose);
        PxShape* shape = GPhysics->createShape(PxCapsuleGeometry(bone.halfSize.x, bone.halfSize.y), *GMaterial);
        body->attachShape(*shape);
        PxRigidBodyExt::updateMassAndInertia(*body, 1.0f);
        GScene->addActor(*body);
        bone.body = body;

        // 조인트 연결
        if (bone.parentIndex >= 0)
        {
            RagdollBone& parent = RagdollBones[bone.parentIndex];

            PxTransform localFrameParent = PxTransform(parent.body->getGlobalPose().getInverse() * PxTransform(bonePos));
            PxTransform localFrameChild = PxTransform(PxVec3(0));

            PxD6Joint* joint = PxD6JointCreate(*GPhysics, parent.body, localFrameParent, bone.body, localFrameChild);

            // 각도 제한 설정
            joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
            joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
            joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
            joint->setTwistLimit(PxJointAngularLimitPair(-PxPi / 4, PxPi / 4));
            joint->setSwingLimit(PxJointLimitCone(PxPi / 6, PxPi / 6));

            bone.joint = joint;
        }
    }
}

void USkeletalMeshComponent::DestroyRagDoll()
{

}

void USkeletalMeshComponent::UpdateRagdoll()
{
    if (bRagDollSimulating)
    {
        //for (auto& bone : RagdollBones)
        //{
        //    PxTransform t = bone.body->getGlobalPose();
        //    PxMat44 m(t);
        //}
        for (auto Body : Bodies)
        {
            FVector Location = Body->GetWorldTransform().GetLocation();
            PxTransform t = { Location.X, Location.Y, Location.Z };
            PxMat44 m(t);
        }
        for (auto Constraint : Constraints)
        {
            //FVector Location = Constraint->GetWorldTransform().GetLocation();
            //PxTransform t = { Location.X, Location.Y, Location.Z };
            //PxMat44 m(t);
        }
    }
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    CreatePhysicsState();

    //FVector test = this->GetRelativeTransform().GetLocation();
    //CreateRagDoll({ test.X, test.Y, test.Z }); // 월드 루트 위치를 기준으로 시작);
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

    PhysicsAssetToPopulate->BodySetup.Empty();
    PhysicsAssetToPopulate->ConstraintSetup.Empty(); // Constraint 설정도 초기화

    // 1. BodySetups 생성
    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    {
        const FMeshBoneInfo& BoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex];
        const FTransform& BoneLocalTransform = InRefSkeleton.GetRawRefBonePose()[BoneIndex]; // 본의 로컬 (부모 상대) 트랜스폼

        UBodySetup* NewBodySetup = FObjectFactory::ConstructObject<UBodySetup>(PhysicsAssetToPopulate); // Outer를 PhysicsAsset으로
        NewBodySetup->BoneName = BoneInfo.Name;

        // --- 콜리전 모양 및 크기 설정 (예시: 캡슐) ---
        FKSphylElem CapsuleElem;
        CapsuleElem.Center = FVector::ZeroVector; // BodySetup 내의 콜리전은 본에 상대적이므로 보통 (0,0,0) 에서 시작
        // 실제 오프셋은 BodySetup의 트랜스폼이나, 본 자체의 트랜스폼으로 관리됨.
        // 또는 본 길이의 절반 지점에 위치시킬 수 있음.

        // 본의 길이와 두께를 기반으로 캡슐 크기 결정 (이 부분은 매우 단순화된 예시)
        float BoneLength = 50.0f; // 실제로는 자식 본 위치나 특정 로직으로 계산
        float Radius = 10.0f;    // 적절한 반지름 값

        // 다음 본과의 거리로 길이를 추정하거나, 스켈레톤 데이터에서 직접 가져올 수 있다면 더 좋음
        if (BoneIndex + 1 < InRefSkeleton.GetRawBoneNum() && InRefSkeleton.GetRawRefBoneInfo()[BoneIndex + 1].ParentIndex == BoneIndex)
        {
            BoneLength = FVector::Dist(BoneLocalTransform.GetLocation(), InRefSkeleton.GetRawRefBonePose()[BoneIndex + 1].GetLocation());
        }
        CapsuleElem.Length = BoneLength - (2 * Radius); // 실제 캡슐 몸통 길이
        CapsuleElem.Radius = Radius;
        // 캡슐 방향 설정: PhysX 캡슐은 기본적으로 X축을 따라 생성. 본의 주축에 맞춰 회전 필요.
        // 여기서는 Z축을 향한다고 가정하고 회전 (엔진 좌표계에 따라 다름)
        CapsuleElem.Rotation = FRotator(90.f, 0.f, 0.f); // 예: Z축으로 세우기 위해 X축 90도 회전

        NewBodySetup->AggGeom.SphylElems.Add(CapsuleElem);
        // --- 콜리전 설정 끝 ---

        PhysicsAssetToPopulate->BodySetup.Add(NewBodySetup);
    }

    // 2. ConstraintSetups 생성
    for (int32 BoneIndex = 0; BoneIndex < InRefSkeleton.GetRawBoneNum(); ++BoneIndex)
    {
        const FMeshBoneInfo& BoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneIndex];
        if (BoneInfo.ParentIndex != INDEX_NONE) // 루트 본이 아니면 부모와 조인트 생성
        {
            const FMeshBoneInfo& ParentBoneInfo = InRefSkeleton.GetRawRefBoneInfo()[BoneInfo.ParentIndex];

            UConstraintSetup* NewConstraintSetup= new UConstraintSetup();
            NewConstraintSetup->JointName = FName(*(BoneInfo.Name.ToString() + TEXT("_joint_") + ParentBoneInfo.Name.ToString()));
            NewConstraintSetup->ConstraintBone1 = ParentBoneInfo.Name; // 부모 본
            NewConstraintSetup->ConstraintBone2 = BoneInfo.Name;   // 자식 본

            // 로컬 프레임 설정: 조인트의 위치와 방향을 각 본의 로컬 공간에서 정의
            // 예: 부모 본의 끝, 자식 본의 시작 부분에 조인트 위치
            //    이 값들은 PhysicsAsset 에디터에서 튜닝하는 것이 일반적임.
            //    여기서는 간단히 Identity로 설정 (실제로는 매우 중요하고 복잡한 부분)
            NewConstraintSetup->LocalFrame1 = FTransform::Identity; // ParentBone의 로컬 공간에서의 조인트 프레임
            NewConstraintSetup->LocalFrame2 = FTransform::Identity; // ChildBone의 로컬 공간에서의 조인트 프레임

            // 각도 제한 설정 (예시)
            NewConstraintSetup->AngularLimits.TwistLimitAngle = 45.f;
            NewConstraintSetup->AngularLimits.Swing1LimitAngle = 30.f;
            NewConstraintSetup->AngularLimits.Swing2LimitAngle = 30.f;

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
                BoneRefPoseGlobalTransform = CurrentSkeleton->GetReferenceSkeleton().GetRawRefBonePose()[BoneIdx];
            }


            // InitBody는 bSimulatePhysics=true로 호출되어야 래그돌처럼 동작 (PxRigidDynamic 생성)
            NewBodyInstance->InitBody(this, Setup, BoneRefPoseGlobalTransform, true /*bSimulatePhysics*/);
            Bodies.Add(NewBodyInstance);
            BoneNameToBodyInstanceMap.Add(Setup->BoneName, NewBodyInstance);

            // 생성된 PxRigidDynamic을 RagdollBones와 유사한 구조에 저장할 수도 있지만,
            // FBodyInstance가 PxRigidActor를 이미 가지고 있음.
        }
    }

    // 3. PhysicsAsset의 ConstraintSetups로부터 FConstraintInstance (및 PxJoint) 생성
    Constraints.Reserve(PhysAsset->ConstraintSetup.Num());
    for (const auto CSSetup : PhysAsset->ConstraintSetup)
    {
        FBodyInstance* BodyInst1 = *BoneNameToBodyInstanceMap.Find(CSSetup->ConstraintBone1);
        FBodyInstance* BodyInst2 = *BoneNameToBodyInstanceMap.Find(CSSetup->ConstraintBone2);

        const FReferenceSkeleton& RefSkeleton = GetSkeletalMeshAsset()->GetSkeleton()->GetReferenceSkeleton();
        CSSetup->LocalFrame1 = RefSkeleton.GetRawRefBonePose()[RefSkeleton.FindBoneIndex(CSSetup->ConstraintBone1)];
        CSSetup->LocalFrame2 = RefSkeleton.GetRawRefBonePose()[RefSkeleton.FindBoneIndex(CSSetup->ConstraintBone2)];

        if (BodyInst1 && BodyInst2)
        {
            if (BodyInst1 && BodyInst2 && BodyInst1->RigidActor && BodyInst2->RigidActor)
            {
                UConstraintInstance* NewConstraintInstance = new UConstraintInstance();
                // InitConstraint는 FConstraintSetup을 직접 받도록 수정했었음
                NewConstraintInstance->InitConstraint(CSSetup, BodyInst1, BodyInst2, this, true /*bSimulatePhysics*/);
                Constraints.Add(NewConstraintInstance);
            }
        }
    }
    // SetSimulatePhysics(true); // 컴포넌트 전체에 대한 플래그. 이미 개별 BodyInstance 생성 시 반영.
}
