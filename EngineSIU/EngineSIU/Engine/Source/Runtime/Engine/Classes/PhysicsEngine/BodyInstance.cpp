// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppClangTidyCppcoreguidelinesProTypeStaticCastDowncast
#include "BodyInstance.h"

#include "BodySetup.h"
#include "PxPhysicsAPI.h"
#include "Components/PrimitiveComponent.h"
#include "extensions/PxRigidBodyExt.h"

using namespace physx;

extern PxFoundation* GFoundation;
extern PxPhysics* GPhysics;
extern PxDefaultCpuDispatcher* GDispatcher;
extern PxScene* GScene;
extern PxMaterial* GMaterial;

struct FKSphereElem;
struct FKBoxElem;
struct FKSphylElem;
struct FKConvexElem;


struct FBodyInstance::FBodyInstancePImpl
{
    PxRigidActor* RigidActor;
    void* UserData;

    // 현재 물리 시뮬레이션 여부
    bool bIsSimulatingPhysics;

    FBodyInstancePImpl()
        : RigidActor(nullptr)
        , UserData(nullptr)
        , bIsSimulatingPhysics(false)
    {
    }

    ~FBodyInstancePImpl()
    {
        if (RigidActor)
        {
            RigidActor->release();
            RigidActor = nullptr;
        }
    }

    // UBodySetup의 FKAggregateGeom 정보를 바탕으로 PxShape들을 생성하고 PxRigidActor에 붙입니다.
    void CreateShapesFromAggGeom(const UBodySetup* BodySetupRef, PxRigidActor* OutActor);

    PxShape* CreateShapeFromSphere(const FKSphereElem& SphereElem, const PxMaterial& Material) const;
    PxShape* CreateShapeFromBox(const FKBoxElem& BoxElem, const PxMaterial& Material) const;
    PxShape* CreateShapeFromSphyl(const FKSphylElem& SphylElem, const PxMaterial& Material) const;
    PxShape* CreateShapeFromConvex(const FKConvexElem& ConvexElem, const PxMaterial& Material) const;
};

void FBodyInstance::FBodyInstancePImpl::CreateShapesFromAggGeom(const UBodySetup* BodySetupRef, PxRigidActor* OutActor)
{
    if (!(BodySetupRef && RigidActor && GMaterial))
    {
        return;
    }

    const FKAggregateGeom& AggGeom = BodySetupRef->AggGeom;

    for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
    {
        if (PxShape* Shape = CreateShapeFromSphere(SphereElem, *GMaterial))
        {
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }

    for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
    {
        if (PxShape* Shape = CreateShapeFromBox(BoxElem, *GMaterial))
        {
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }

    for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
    {
        if (PxShape* Shape = CreateShapeFromSphyl(SphylElem, *GMaterial))
        {
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }

    for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
    {
        if (PxShape* Shape = CreateShapeFromConvex(ConvexElem, *GMaterial))
        {
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }
}

PxShape* FBodyInstance::FBodyInstancePImpl::CreateShapeFromSphere(const FKSphereElem& SphereElem, const PxMaterial& Material) const
{
    PxShape* Shape = GPhysics->createShape(
        PxSphereGeometry(SphereElem.Radius), Material, true, PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE
    );

    if (Shape)
    {
        const PxTransform LocalPose{PxVec3{SphereElem.Center.X, SphereElem.Center.Y, SphereElem.Center.Z}};
        Shape->setLocalPose(LocalPose);
    }

    return Shape;
}

PxShape* FBodyInstance::FBodyInstancePImpl::CreateShapeFromBox(const FKBoxElem& BoxElem, const PxMaterial& Material) const
{
    PxShape* Shape = GPhysics->createShape(
        PxBoxGeometry(BoxElem.X, BoxElem.Y, BoxElem.Z), // 박스 지오메트리 (Half-Extents)
        Material,
        true,
        PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE
    );

    if (Shape)
    {
        const FQuat EngineQuat = BoxElem.Rotation.Quaternion();
        const PxQuat PxQuatRotation{EngineQuat.X, EngineQuat.Y, EngineQuat.Z, EngineQuat.W};
        const PxTransform LocalPose{
            PxVec3{BoxElem.Center.X, BoxElem.Center.Y, BoxElem.Center.Z},
            PxQuatRotation
        };
        Shape->setLocalPose(LocalPose);
    }
    return Shape;
}

PxShape* FBodyInstance::FBodyInstancePImpl::CreateShapeFromSphyl(const FKSphylElem& SphylElem, const PxMaterial& Material) const
{
    PxShape* Shape = GPhysics->createShape(
        PxCapsuleGeometry(SphylElem.Radius, SphylElem.Length / 2.0f), // 캡슐 지오메트리
        Material,
        true,
        PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE
    );

    if (Shape)
    {
        const FQuat EngineQuat = SphylElem.Rotation.Quaternion();
        // PhysX 캡슐은 X축 기준이므로, 필요시 엔진 좌표계에 맞춰 추가 회전 적용
        // 예: EngineQuat = EngineQuat * FQuat(FVector::UpVector, -PI / 2.0f) * FQuat(FVector::RightVector, PI / 2.0f);

        const PxQuat PxQuatRotation(EngineQuat.X, EngineQuat.Y, EngineQuat.Z, EngineQuat.W);
        const PxTransform LocalPose(
            PxVec3(SphylElem.Center.X, SphylElem.Center.Y, SphylElem.Center.Z),
            PxQuatRotation
        );
        Shape->setLocalPose(LocalPose);
    }
    return Shape;
}

PxShape* FBodyInstance::FBodyInstancePImpl::CreateShapeFromConvex(const FKConvexElem& ConvexElem, const PxMaterial& Material) const
{
    return nullptr;
}

FBodyInstance::FBodyInstance()
    : OwnerComponent(nullptr)
    , PImpl(std::make_unique<FBodyInstancePImpl>())
{
}

FBodyInstance::~FBodyInstance()
{
    TermBody();
    PImpl.reset();
}

FBodyInstance::FBodyInstance(FBodyInstance&& Other) noexcept
    : OwnerComponent(Other.OwnerComponent)
    , PImpl(std::move(Other.PImpl))
{
    Other.OwnerComponent = nullptr;
    Other.PImpl = nullptr;
}

FBodyInstance& FBodyInstance::operator=(FBodyInstance&& Other) noexcept
{
    if (this != &Other)
    {
        PImpl = std::move(Other.PImpl);
        OwnerComponent = Other.OwnerComponent;

        Other.PImpl = nullptr;
        Other.OwnerComponent = nullptr;
    }
    return *this;
}

void FBodyInstance::InitBody(
    UPrimitiveComponent* InOwnerComponent, const UBodySetup* InBodySetup, const FTransform& InTransform, bool bInSimulatePhysics
)
{
    if (!(PImpl && GPhysics && GScene && GMaterial && InBodySetup && InOwnerComponent))
    {
        // UE_LOG: 필수 객체 없음
        return;
    }

    // 이미 초기화되었다면 기존 것 해제
    if (PImpl->RigidActor)
    {
        TermBody();
        // PImpl은 유지하고 내부 RigidActor만 새로 만듦
        PImpl->RigidActor = nullptr; // 명시적 초기화
    }

    OwnerComponent = InOwnerComponent;
    PImpl->bIsSimulatingPhysics = bInSimulatePhysics;

    const FVector Location = InTransform.GetLocation();
    const FQuat Rotation = InTransform.GetRotation();

    const PxTransform PxPose(
        PxVec3(Location.X, Location.Y, Location.Z),
        PxQuat(
            Rotation.X, Rotation.Y, Rotation.Z, Rotation.W
        )
    );

    if (PImpl->bIsSimulatingPhysics)
    {
        PImpl->RigidActor = GPhysics->createRigidDynamic(PxPose);
    }
    else
    {
        PImpl->RigidActor = GPhysics->createRigidStatic(PxPose);
    }

    if (!PImpl->RigidActor)
    {
        // UE_LOG: RigidActor 생성 실패
        return;
    }

    // userData에 FBodyInstance* this를 저장 (PhysX 콜백 등에서 다시 FBodyInstance를 얻기 위함)
    PImpl->RigidActor->userData = this;

    PImpl->CreateShapesFromAggGeom(InBodySetup, PImpl->RigidActor);

    if (PImpl->bIsSimulatingPhysics && PImpl->RigidActor->is<PxRigidDynamic>())
    {
        // TODO: BodySetupRef 또는 OwnerComponent에서 질량/관성 관련 데이터 가져오기
        float Mass = 0.001f;
        if (InBodySetup && InBodySetup->Mass > 0)
        {
            Mass = InBodySetup->Mass; 
        }
        PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidDynamic*>(PImpl->RigidActor), Mass);
    }

    GScene->addActor(*(PImpl->RigidActor));
}

void FBodyInstance::TermBody()
{
    if (PImpl && PImpl->RigidActor)
    {
        if (GScene)
        {
            GScene->removeActor(*(PImpl->RigidActor));
        }
        PImpl->RigidActor->release();
        PImpl->RigidActor = nullptr;
    }
    // OwnerComponent는 TermBody에서 null로 만들지 않음. InitBody에서 새로 설정.
    // 또는 FBodyInstance 소멸 시 OwnerComponent = nullptr; 처리.
}

void FBodyInstance::SyncPhysXToComponent()
{
    if (PImpl && PImpl->RigidActor && OwnerComponent && PImpl->bIsSimulatingPhysics) // 물리 시뮬레이션 중일 때만 동기화
    {
        PxSceneReadLock ScopedReadLock{*GScene};

        const PxTransform PxPose = PImpl->RigidActor->getGlobalPose();
        const FVector NewLocation(PxPose.p.x, PxPose.p.y, PxPose.p.z);
        const FQuat NewRotation(PxPose.q.x, PxPose.q.y, PxPose.q.z, PxPose.q.w);

        OwnerComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
    }
}

void FBodyInstance::SyncComponentToPhysX()
{
    if (PImpl && PImpl->RigidActor && OwnerComponent && PImpl->bIsSimulatingPhysics)
    {
        PxSceneWriteLock ScopedWriteLock{*GScene};

        const FTransform NewTransform = OwnerComponent->GetComponentTransform();
        const FVector NewLocation = NewTransform.GetLocation();
        const FQuat NewRotation = NewTransform.GetRotation();

        const PxTransform PxNewPose{
            PxVec3(NewLocation.X, NewLocation.Y, NewLocation.Z),
            PxQuat(NewRotation.X, NewRotation.Y, NewRotation.Z, NewRotation.W)
        };

        PImpl->RigidActor->setGlobalPose(PxNewPose);
    }
}

bool FBodyInstance::IsValidBodyInstance() const
{
    return PImpl && PImpl->RigidActor;
}

void FBodyInstance::AddForce(const FVector& Force, bool bAccelChange)
{
    if (PImpl && PImpl->RigidActor && PImpl->RigidActor->is<PxRigidBody>())
    {
        PxRigidBody* RigidBody = static_cast<PxRigidBody*>(PImpl->RigidActor);
        const PxVec3 PxForce(Force.X, Force.Y, Force.Z);
        const PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
        RigidBody->addForce(PxForce, Mode);
    }
}

void FBodyInstance::SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent)
{
    if (PImpl && PImpl->RigidActor && PImpl->RigidActor->is<PxRigidBody>())
    {
        PxRigidBody* RigidBody = static_cast<PxRigidBody*>(PImpl->RigidActor);
        PxVec3 PxNewVel(NewVel.X, NewVel.Y, NewVel.Z);
        if (bAddToCurrent)
        {
            PxNewVel += RigidBody->getLinearVelocity();
        }
        RigidBody->setLinearVelocity(PxNewVel);
    }
}

FVector FBodyInstance::GetLinearVelocity() const
{
    if (PImpl && PImpl->RigidActor && PImpl->RigidActor->is<PxRigidBody>())
    {
        const PxVec3 PxVel = static_cast<PxRigidBody*>(PImpl->RigidActor)->getLinearVelocity();
        return FVector{PxVel.x, PxVel.y, PxVel.z};
    }
    return FVector::ZeroVector;
}

void FBodyInstance::SetAngularVelocity(const FVector& NewAngVel, bool bAddToCurrent)
{
    if (PImpl && PImpl->RigidActor && PImpl->RigidActor->is<PxRigidBody>())
    {
        PxRigidBody* RigidBody = static_cast<PxRigidBody*>(PImpl->RigidActor);
        PxVec3 PxNewAngVel(NewAngVel.X, NewAngVel.Y, NewAngVel.Z); // 엔진 좌표계와 PhysX 좌표계가 다르면 변환 필요
        if (bAddToCurrent)
        {
            PxNewAngVel += RigidBody->getAngularVelocity();
        }
        RigidBody->setAngularVelocity(PxNewAngVel);
    }
}

FVector FBodyInstance::GetAngularVelocity() const
{
    if (PImpl && PImpl->RigidActor && PImpl->RigidActor->is<PxRigidBody>())
    {
        const PxVec3 PxAngVel = static_cast<PxRigidBody*>(PImpl->RigidActor)->getAngularVelocity();
        return FVector{PxAngVel.x, PxAngVel.y, PxAngVel.z}; // 좌표계 변환 필요할 수 있음
    }
    return FVector::ZeroVector;
}

FTransform FBodyInstance::GetWorldTransform() const
{
    if (PImpl && PImpl->RigidActor)
    {
        const PxTransform PxPose = PImpl->RigidActor->getGlobalPose();
        return FTransform{
            FQuat(PxPose.q.x, PxPose.q.y, PxPose.q.z, PxPose.q.w),
            FVector(PxPose.p.x, PxPose.p.y, PxPose.p.z)
        };
    }
    return FTransform::Identity;
}

void FBodyInstance::SetWorldTransform(const FTransform& NewTransform, bool bTeleportPhysics)
{
    if (PImpl && PImpl->RigidActor)
    {
        const PxTransform PxNewPose(
            PxVec3(NewTransform.GetLocation().X, NewTransform.GetLocation().Y, NewTransform.GetLocation().Z),
            PxQuat(NewTransform.GetRotation().X, NewTransform.GetRotation().Y, NewTransform.GetRotation().Z, NewTransform.GetRotation().W)
        );

        if (PImpl->RigidActor->is<PxRigidDynamic>())
        {
            // bTeleportPhysics 플래그는 PhysX에서 직접적으로 지원하는 것은 아니지만,
            // Kinematic Actor의 경우 setKinematicTarget, Dynamic Actor의 경우 setGlobalPose 후 wakeUp 등을 의미할 수 있음.
            // 간단히 setGlobalPose 사용. Kinematic인 경우 추가 처리가 필요할 수 있음.
            PImpl->RigidActor->setGlobalPose(PxNewPose);
            if (!bTeleportPhysics) // 순간이동이 아니면 깨움 (물리가 즉시 반응하도록)
            {
                static_cast<PxRigidDynamic*>(PImpl->RigidActor)->wakeUp();
            }
        }
        else if (PImpl->RigidActor->is<PxRigidStatic>())
        {
            // Static Actor는 런타임에 Transform 변경 불가 (만약 필요하면 재생성해야 함)
            // UE_LOG: Static Actor의 Transform은 변경할 수 없습니다.
        }
    }
}

void FBodyInstance::SetSimulatePhysics(bool bSimulate)
{
    if (PImpl && PImpl->bIsSimulatingPhysics != bSimulate)
    {
        PImpl->bIsSimulatingPhysics = bSimulate;
        if (PImpl->RigidActor)
        {
            // TODO: 실제로는 Actor Type을 Dynamic <-> Static으로 변경하거나,
            // Dynamic Actor의 플래그를 변경하는 복잡한 과정이 필요할 수 있습니다.
            // (예: PxRigidBodyFlag::eKINEMATIC 설정/해제)
            // 여기서는 단순 플래그만 변경하고, InitBody 시점에 실제 Actor Type이 결정된다고 가정합니다.
            // 혹은, 해당 플래그에 따라 Actor를 재생성해야 할 수도 있습니다.
            if (PImpl->RigidActor->is<PxRigidDynamic>())
            {
                // 물리 시뮬레이션을 끄면 Kinematic으로 만들거나 Gravity를 끌 수 있음
                // 여기서는 간단히 Actor flag를 조절한다고 가정 (예시)
                // static_cast<physx::PxRigidDynamic*>(PImpl->RigidActor)->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, !bSimulate);
            }
            // UE_LOG: SetSimulatePhysics 호출됨, 현재 상태: %s", bSimulate ? "On" : "Off");
        }
    }
}

bool FBodyInstance::IsSimulatingPhysics() const
{
    return PImpl && PImpl->bIsSimulatingPhysics;
}


void FBodyInstance::SetUserData(void* InUserData)
{
    if (PImpl)
    {
        PImpl->UserData = InUserData;
    }
}

void* FBodyInstance::GetUserData() const
{
    return PImpl ? PImpl->UserData : nullptr;
}
