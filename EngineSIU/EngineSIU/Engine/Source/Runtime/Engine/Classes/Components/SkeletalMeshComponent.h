#pragma once
#include "SkinnedMeshComponent.h"
#include "Actors/Player.h"
#include "Engine/AssetManager.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Template/SubclassOf.h"
#include "Animation/AnimNodeBase.h"

#include "PxPhysicsAPI.h"

struct FConstraintInstance;
class UAnimSequence;
class USkeletalMesh;
struct FAnimNotifyEvent;
class UAnimSequenceBase;
class UAnimInstance;
class UAnimSingleNodeInstance;
class UPhysicsAsset;

struct RagdollBone
{
    FName name;
    physx::PxVec3 offset;                // 부모로부터의 위치
    physx::PxVec3 halfSize;              // Capsule or box 크기
    int parentIndex;              // -1이면 루트
    physx::PxRigidDynamic* body = nullptr;
    physx::PxJoint* joint = nullptr;
};


enum class EAnimationMode : uint8
{
    AnimationBlueprint,
    AnimationSingleNode,
};

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    USkeletalMeshComponent();
    virtual ~USkeletalMeshComponent() override = default;

    virtual void InitializeComponent() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void TickComponent(float DeltaTime) override;
    virtual void PhysicsTick() override;

    virtual void TickPose(float DeltaTime) override;

    void TickAnimation(float DeltaTime);

    void TickAnimInstances(float DeltaTime);

    bool ShouldTickAnimation() const;

    bool InitializeAnimScriptInstance();

    void ClearAnimScriptInstance();

    USkeletalMesh* GetSkeletalMeshAsset() const { return SkeletalMeshAsset; }

    void SetSkeletalMeshAsset(USkeletalMesh* InSkeletalMeshAsset);

    FTransform GetSocketTransform(FName SocketName) const;

    TArray<FTransform> RefBonePoseTransforms; // 원본 BindPose에서 복사해온 에디팅을 위한 Transform

    void GetCurrentGlobalBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const;

    void DEBUG_SetAnimationEnabled(bool bEnable);

    void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);

    void SetAnimation(UAnimationAsset* NewAnimToPlay);

    UAnimationAsset* GetAnimation() const;

    void Play(bool bLooping);

    void Stop();

    void SetPlaying(bool bPlaying);
    
    bool IsPlaying() const;

    void SetReverse(bool bIsReverse);
    
    bool IsReverse() const;

    void SetPlayRate(float Rate);

    float GetPlayRate() const;

    void SetLooping(bool bIsLooping);

    bool IsLooping() const;

    int GetCurrentKey() const;

    void SetCurrentKey(int InKey);

    void SetElapsedTime(float InElapsedTime);

    float GetElapsedTime() const;

    int32 GetLoopStartFrame() const;

    void SetLoopStartFrame(int32 InLoopStartFrame);

    int32 GetLoopEndFrame() const;

    void SetLoopEndFrame(int32 InLoopEndFrame);
    
    bool bIsAnimationEnabled() const { return bPlayAnimation; }
    
    virtual int CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const override;

    const FSkeletalMeshRenderData* GetCPURenderData() const;

    static void SetCPUSkinning(bool Flag);

    static bool GetCPUSkinning();

    UAnimInstance* GetAnimInstance() const { return AnimScriptInstance; }

    void SetAnimationMode(EAnimationMode InAnimationMode);

    EAnimationMode GetAnimationMode() const { return AnimationMode; }

    virtual void InitAnim();
    
protected:
    bool NeedToSpawnAnimScriptInstance() const;

    EAnimationMode AnimationMode;

public:
    /** Array of FBodyInstance objects, storing per-instance state about about each body. */
    TArray<FBodyInstance*> Bodies;

    /** Array of FConstraintInstance structs, storing per-instance state about each constraint. */
    TArray<FConstraintInstance*> Constraints;

    // 물리 상태 생성/파괴
    //virtual bool ShouldCreatePhysicsState() const;
    virtual void CreatePhysicsState() override;
    virtual void DestroyPhysicsState() override;

protected:
    void ClearPhysicsState();

    void InstantiatePhysicsAsset();

    void SyncBodiesToBones();

private:
    FPoseContext BonePoseContext;
    
    USkeletalMesh* SkeletalMeshAsset;

    bool bPlayAnimation;

    std::unique_ptr<FSkeletalMeshRenderData> CPURenderData;

    static bool bIsCPUSkinning;

    void CPUSkinning(bool bForceUpdate = false);

public:
    TSubclassOf<UAnimInstance> AnimClass;
    
    UAnimInstance* AnimScriptInstance;

    UAnimSingleNodeInstance* GetSingleNodeInstance() const;

    void SetAnimClass(UClass* NewClass);
    
    UClass* GetAnimClass();
    
    void SetAnimInstanceClass(class UClass* NewClass);

private:
    UPROPERTY(
        EditAnywhere | LuaReadOnly, ({ .Category = "RagDoll" }),
        bool, bRagDollSimulating, ;
    )

    TArray<RagdollBone> RagdollBones;

public:
    void SetRagDollSimulating(bool bInRagDollSimulating){ bRagDollSimulating = bInRagDollSimulating; }
    bool IsRagDollSimulating() const { return bRagDollSimulating; }

    void InitializeRagDoll(const FReferenceSkeleton& InRefSkeleton);
    //void CreateRagDoll(const physx::PxVec3& WorldRoot);
    void DestroyRagDoll();

    void UpdateRagdoll();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void PopulatePhysicsAssetFromSkeleton(UPhysicsAsset* PhysicsAssetToPopulate, const FReferenceSkeleton& InRefSkeleton);
    void CreateRagDollFromPhysicsAsset();

private:
    const float DefaultBodyMass = 0.05f;
    const float MinRadius = 0.1f;                     // 캡슐의 최소 반지름
    const float MaxRadius = 3.0f;                     // 캡슐의 최대 반지름
    const float DefaultRadius = 0.5f;                 // 기본 반지름
    const float MinCylinderLength = 0.01f;             // 캡슐의 원통 부분 최소 길이
    const float DefaultBoneLength = 2.0f;            // 자식이 없거나 길이가 매우 짧은 본의 기본 길이
    float CalculatedRadius = 0.5f; //DefaultRadius
    float CalculatedCylinderLength = 2.f; // 원통 부분의 길이 (DefaultBoneLength)

    void CalculateElement(UBodySetup* InBodySetup, const FReferenceSkeleton& InRefSkeleton, int32 BoneIndex);

};
