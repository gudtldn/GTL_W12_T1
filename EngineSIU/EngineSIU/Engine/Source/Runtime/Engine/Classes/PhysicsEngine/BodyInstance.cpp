// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppClangTidyCppcoreguidelinesProTypeStaticCastDowncast
#include "BodyInstance.h"
#include "PhysX/PhysX.h"

#include "BodySetup.h"
#include "PxPhysicsAPI.h"
#include "Components/PrimitiveComponent.h"
#include "extensions/PxRigidBodyExt.h"

using namespace physx;


struct FKSphereElem;
struct FKBoxElem;
struct FKSphylElem;
struct FKConvexElem;


void FBodyInstance::CreateShapesFromAggGeom(const UBodySetup* BodySetupRef, PxRigidActor* OutActor)
{
    if (!(BodySetupRef && RigidActor && FPhysX::GMaterial))
    {
        return;
    }

    PxFilterData ShapeFilterData = BodySetupRef->CreateFilterData();
    const FKAggregateGeom& AggGeom = BodySetupRef->AggGeom;

    for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
    {
        if (PxShape* Shape = CreateShapeFromSphere(SphereElem, *FPhysX::GMaterial))
        {
            Shape->setSimulationFilterData(ShapeFilterData);
            Shape->setQueryFilterData(ShapeFilterData);
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }

    for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
    {
        if (PxShape* Shape = CreateShapeFromBox(BoxElem, *FPhysX::GMaterial))
        {
            Shape->setSimulationFilterData(ShapeFilterData);
            Shape->setQueryFilterData(ShapeFilterData);
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }

    for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
    {
        if (PxShape* Shape = CreateShapeFromSphyl(SphylElem, *FPhysX::GMaterial))
        {
            Shape->setSimulationFilterData(ShapeFilterData);
            Shape->setQueryFilterData(ShapeFilterData);
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }

    for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
    {
        if (PxShape* Shape = CreateShapeFromConvex(ConvexElem, *FPhysX::GMaterial))
        {
            Shape->setSimulationFilterData(ShapeFilterData);
            Shape->setQueryFilterData(ShapeFilterData);
            OutActor->attachShape(*Shape);
            Shape->release();
        }
    }
}

PxShape* FBodyInstance::CreateShapeFromSphere(const FKSphereElem& SphereElem, const PxMaterial& Material) const
{
    PxShape* Shape = FPhysX::GPhysics->createShape(
        PxSphereGeometry(SphereElem.Radius), Material, true, PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE
    );

    if (Shape)
    {
        const PxTransform LocalPose{PxVec3{SphereElem.Center.X, SphereElem.Center.Y, SphereElem.Center.Z}};
        Shape->setLocalPose(LocalPose);
    }

    return Shape;
}

PxShape* FBodyInstance::CreateShapeFromBox(const FKBoxElem& BoxElem, const PxMaterial& Material) const
{
    PxShape* Shape = FPhysX::GPhysics->createShape(
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

PxShape* FBodyInstance::CreateShapeFromSphyl(const FKSphylElem& SphylElem, const PxMaterial& Material) const
{
    PxShape* Shape = FPhysX::GPhysics->createShape(
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

PxShape* FBodyInstance::CreateShapeFromConvex(const FKConvexElem& ConvexElem, const PxMaterial& Material) const
{
    return nullptr;
}

FBodyInstance::FBodyInstance()
    : OwnerComponent(nullptr)
    , RigidActor(nullptr)
    , bIsSimulatingPhysics(false)
{
}

FBodyInstance::~FBodyInstance()
{
    TermBody();
}

FBodyInstance::FBodyInstance(FBodyInstance&& Other) noexcept = default;
FBodyInstance& FBodyInstance::operator=(FBodyInstance&& Other) noexcept = default;

void FBodyInstance::InitBody(
    UPrimitiveComponent* InOwnerComponent, const UBodySetup* InBodySetup, const FTransform& InTransform, bool bInSimulatePhysics
)
{
    if (!(FPhysX::GPhysics && FPhysX::GScene && FPhysX::GMaterial && InBodySetup && InOwnerComponent))
    {
        // UE_LOG: 필수 객체 없음
        return;
    }

    // 이미 초기화되었다면 기존 것 해제
    if (RigidActor)
    {
        TermBody();
        // PImpl은 유지하고 내부 RigidActor만 새로 만듦
        RigidActor = nullptr; // 명시적 초기화
    }

    OwnerComponent = InOwnerComponent;
    bIsSimulatingPhysics = bInSimulatePhysics;

    const FVector Location = InTransform.GetLocation();
    const FQuat Rotation = InTransform.GetRotation();

    const PxTransform PxPose(
        PxVec3(Location.X, Location.Y, Location.Z),
        PxQuat(
            Rotation.X, Rotation.Y, Rotation.Z, Rotation.W
        )
    );

    if (bIsSimulatingPhysics)
    {
        RigidActor = FPhysX::GPhysics->createRigidDynamic(PxPose);
    }
    else
    {
        RigidActor = FPhysX::GPhysics->createRigidStatic(PxPose);
    }

    if (!RigidActor)
    {
        // UE_LOG: RigidActor 생성 실패
        return;
    }

    // userData에 FBodyInstance* this를 저장 (PhysX 콜백 등에서 다시 FBodyInstance를 얻기 위함)
    RigidActor->userData = this;

    CreateShapesFromAggGeom(InBodySetup, RigidActor);

    if (bIsSimulatingPhysics && RigidActor->is<PxRigidDynamic>())
    {
        // TODO: BodySetupRef 또는 OwnerComponent에서 질량/관성 관련 데이터 가져오기
        float Mass = 0.001f;
        if (InBodySetup && InBodySetup->Mass > 0)
        {
            Mass = InBodySetup->Mass; 
        }
        PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidDynamic*>(RigidActor), Mass);
    }

    FPhysX::GScene->addActor(*(RigidActor));
}

void FBodyInstance::TermBody()
{
    if (RigidActor)
    {
        if (FPhysX::GScene)
        {
            FPhysX::GScene->removeActor(*(RigidActor));
        }
        RigidActor->release();
        RigidActor = nullptr;
    }
}

void FBodyInstance::SyncPhysXToComponent()
{
    if (RigidActor && OwnerComponent && bIsSimulatingPhysics) // 물리 시뮬레이션 중일 때만 동기화
    {
        PxSceneReadLock ScopedReadLock{*FPhysX::GScene};

        const PxTransform PxPose = RigidActor->getGlobalPose();
        const FVector NewLocation(PxPose.p.x, PxPose.p.y, PxPose.p.z);
        const FQuat NewRotation(PxPose.q.x, PxPose.q.y, PxPose.q.z, PxPose.q.w);

        OwnerComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
    }
}

void FBodyInstance::SyncComponentToPhysX()
{
    if (RigidActor && OwnerComponent && bIsSimulatingPhysics)
    {
        PxSceneWriteLock ScopedWriteLock{*FPhysX::GScene};

        const FTransform NewTransform = OwnerComponent->GetComponentTransform();
        const FVector NewLocation = NewTransform.GetLocation();
        const FQuat NewRotation = NewTransform.GetRotation();

        const PxTransform PxNewPose{
            PxVec3(NewLocation.X, NewLocation.Y, NewLocation.Z),
            PxQuat(NewRotation.X, NewRotation.Y, NewRotation.Z, NewRotation.W)
        };

        RigidActor->setGlobalPose(PxNewPose);
    }
}

bool FBodyInstance::IsValidBodyInstance() const
{
    return RigidActor;
}

void FBodyInstance::AddForce(const FVector& Force, bool bAccelChange)
{
    if (RigidActor && RigidActor->is<PxRigidBody>())
    {
        PxRigidBody* RigidBody = static_cast<PxRigidBody*>(RigidActor);
        const PxVec3 PxForce(Force.X, Force.Y, Force.Z);
        const PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
        RigidBody->addForce(PxForce, Mode);
    }
}

void FBodyInstance::SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent)
{
    if (RigidActor && RigidActor->is<PxRigidBody>())
    {
        PxRigidBody* RigidBody = static_cast<PxRigidBody*>(RigidActor);
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
    if (RigidActor && RigidActor->is<PxRigidBody>())
    {
        const PxVec3 PxVel = static_cast<PxRigidBody*>(RigidActor)->getLinearVelocity();
        return FVector{PxVel.x, PxVel.y, PxVel.z};
    }
    return FVector::ZeroVector;
}

void FBodyInstance::SetAngularVelocity(const FVector& NewAngVel, bool bAddToCurrent)
{
    if (RigidActor && RigidActor->is<PxRigidBody>())
    {
        PxRigidBody* RigidBody = static_cast<PxRigidBody*>(RigidActor);
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
    if (RigidActor && RigidActor->is<PxRigidBody>())
    {
        const PxVec3 PxAngVel = static_cast<PxRigidBody*>(RigidActor)->getAngularVelocity();
        return FVector{PxAngVel.x, PxAngVel.y, PxAngVel.z}; // 좌표계 변환 필요할 수 있음
    }
    return FVector::ZeroVector;
}

FTransform FBodyInstance::GetWorldTransform() const
{
    if (RigidActor)
    {
        const PxTransform PxPose = RigidActor->getGlobalPose();
        return FTransform{
            FQuat(PxPose.q.x, PxPose.q.y, PxPose.q.z, PxPose.q.w),
            FVector(PxPose.p.x, PxPose.p.y, PxPose.p.z)
        };
    }
    return FTransform::Identity;
}

void FBodyInstance::SetWorldTransform(const FTransform& NewTransform, bool bTeleportPhysics)
{
    if (RigidActor)
    {
        const PxTransform PxNewPose(
            PxVec3(NewTransform.GetLocation().X, NewTransform.GetLocation().Y, NewTransform.GetLocation().Z),
            PxQuat(NewTransform.GetRotation().X, NewTransform.GetRotation().Y, NewTransform.GetRotation().Z, NewTransform.GetRotation().W)
        );

        if (RigidActor->is<PxRigidDynamic>())
        {
            // bTeleportPhysics 플래그는 PhysX에서 직접적으로 지원하는 것은 아니지만,
            // Kinematic Actor의 경우 setKinematicTarget, Dynamic Actor의 경우 setGlobalPose 후 wakeUp 등을 의미할 수 있음.
            // 간단히 setGlobalPose 사용. Kinematic인 경우 추가 처리가 필요할 수 있음.
            RigidActor->setGlobalPose(PxNewPose);
            if (!bTeleportPhysics) // 순간이동이 아니면 깨움 (물리가 즉시 반응하도록)
            {
                static_cast<PxRigidDynamic*>(RigidActor)->wakeUp();
            }
        }
        else if (RigidActor->is<PxRigidStatic>())
        {
            // Static Actor는 런타임에 Transform 변경 불가 (만약 필요하면 재생성해야 함)
            // UE_LOG: Static Actor의 Transform은 변경할 수 없습니다.
        }
    }
}

void FBodyInstance::SetSimulatePhysics(bool bSimulate)
{
    if (bIsSimulatingPhysics != bSimulate)
    {
        bIsSimulatingPhysics = bSimulate;
        if (RigidActor)
        {
            // TODO: 실제로는 Actor Type을 Dynamic <-> Static으로 변경하거나,
            // Dynamic Actor의 플래그를 변경하는 복잡한 과정이 필요할 수 있습니다.
            // (예: PxRigidBodyFlag::eKINEMATIC 설정/해제)
            // 여기서는 단순 플래그만 변경하고, InitBody 시점에 실제 Actor Type이 결정된다고 가정합니다.
            // 혹은, 해당 플래그에 따라 Actor를 재생성해야 할 수도 있습니다.
            if (RigidActor->is<PxRigidDynamic>())
            {
                // 물리 시뮬레이션을 끄면 Kinematic으로 만들거나 Gravity를 끌 수 있음
                // 여기서는 간단히 Actor flag를 조절한다고 가정 (예시)
                // static_cast<physx::PxRigidDynamic*>(RigidActor)->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, !bSimulate);
            }
            // UE_LOG: SetSimulatePhysics 호출됨, 현재 상태: %s", bSimulate ? "On" : "Off");
        }
    }
}

bool FBodyInstance::IsSimulatingPhysics() const
{
    return bIsSimulatingPhysics;
}
