
#include "BodyInstance.h"
#include "PhysicsEngine/PhysScene.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetupCore.h"
#include "PhysicsEngine/BodySetup.h"

// #include <PxPhysicsAPI.h>

bool FBodyInstance::InitBody(AActor* InOwningActor, UPrimitiveComponent* InOwnerComponent, UBodySetup* InBodySetup, FPhysScene* InScene, const FTransform& InInitialTransform, const FPhysicsAggregateHandle& InAggregate)
{
    if (RigidActor)
    {
        UE_LOG(ELogLevel::Warning, TEXT("FBodyInstance::InitBody: RigidActor already exists. Call TermBody first."));
        TermBody();
    }

    if (!InBodySetup || !InScene || !InScene->IsValid() || !InOwnerComponent)
    {
        UE_LOG(ELogLevel::Error, TEXT("FBodyInstance::InitBody: Invalid parameters (BodySetup, Scene, or OwningComponent)."));
        return false;
    }

    this->BodySetup = InBodySetup;
    this->OwnerComponent = InOwnerComponent;
    this->bSimulatePhysics = InOwnerComponent->ShouldSimulatePhysics();

    physx::PxPhysics* PxSDK = gPhysics;
    if (!PxSDK)
    {
        UE_LOG(ELogLevel::Error, TEXT("FBodyInstance::InitBody: Failed to get PhysX SDK instance."));
        return false;
    }

    physx::PxTransform InitialPxTransform = ConvertUnrealTransformToPx(InInitialTransform);

    if (bSimulatePhysics)
    {
        RigidActor = PxSDK->createRigidDynamic(InitialPxTransform);
    }
    else 
    {
        bool bIsKinematic = InOwnerComponent->IsKinematic();
        if (bIsKinematic)
        {
            RigidActor = PxSDK->createRigidDynamic(InitialPxTransform);
            if (RigidActor)
            {
                static_cast<physx::PxRigidBody*>(RigidActor)->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
            }
        }
        else
        {
            RigidActor = PxSDK->createRigidStatic(InitialPxTransform);
        }
    }

    if (!RigidActor)
    {
        UE_LOG(ELogLevel::Error, TEXT("FBodyInstance::InitBody: Failed to create PxRigidActor."));
        return false;
    }

    RigidActor->userData = this;
    const FKAggregateGeom& AggGeom = BodySetup->AggGeom;

    // 현재는 Sphere만 추가한 상태임.
    for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
    {
        physx::PxSphereGeometry PxGeom(SphereElem.Radius);
        // SphereElem.Center는 로컬 트랜스폼으로 적용해야 함
        physx::PxTransform PxShapeLocalPose = ConvertUnrealTransformToPx(SphereElem.GetRelativeTransform());
        physx::PxShape* NewShape = PxSDK->createShape(PxGeom, *PxMat, true);
        if (NewShape)
        {
            NewShape->setLocalPose(PxShapeLocalPose);
            // 충돌 필터 데이터 설정 (BodySetup 또는 컴포넌트의 콜리전 설정 기반)
            // NewShape->setSimulationFilterData(...);
            // NewShape->setQueryFilterData(...);
            NewShape->userData = this;

            RigidActor->attachShape(*NewShape);
            NewShape->release();
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("FBodyInstance::InitBody: Failed to create PxShape (Sphere)."));
        }
    }

    if (bSimulatePhysics && RigidActor->getConcreteType() == physx::PxConcreteType::eRIGID_DYNAMIC)
    {
        physx::PxRigidDynamic* DynActor = static_cast<physx::PxRigidDynamic*>(RigidActor);

        // 질량 및 관성 설정
        // float Mass = GetMass();
        // DynActor->setMass(Mass);
        // physx::PxVec3 InertiaTensor = GetInertiaTensor(); // 관성 텐서 계산 또는 가져오기
        // DynActor->setMassSpaceInertiaTensor(InertiaTensor);
        // 또는 PxRigidBodyExt::updateMassAndInertia(*DynActor, Density) 같은 유틸리티 사용

        // 댐핑 설정
        // DynActor->setLinearDamping(GetLinearDamping());
        // DynActor->setAngularDamping(GetAngularDamping());

        // 기타 플래그 (락 플래그 등)
        // DynActor->setRigidDynamicLockFlags(...);
    }


    if (InAggregate.IsValid())
    {
        physx::PxAggregate* PxAgg = InAggregate.GetUnderlyingAggregate();
        if (PxAgg)
        {
            PxAgg->addActor(*RigidActor);
        }
    }

    InScene->GetPxScene()->addActor(*RigidActor);
    bBodyInitialized = true;

    return true;
}

void FBodyInstance::TermBody()
{
    if (RigidActor)
    {
        physx::PxAggregate* Agg = RigidActor->getAggregate();
        if (Agg) { Agg->removeActor(*RigidActor); }

        physx::PxScene* Scene = RigidActor->getScene();
        if (Scene) { Scene->removeActor(*RigidActor); }

        RigidActor->release();
        RigidActor = nullptr;
    }


    BodySetup = nullptr;
    bBodyInitialized = false;
}

bool FBodyInstance::IsInstanceKinematic() const
{
    return false;
}

physx::PxTransform FBodyInstance::ConvertUnrealTransformToPx(const FTransform& UnrealTransform)
{
    physx::PxQuat PxRot(UnrealTransform.GetRotation().X, UnrealTransform.GetRotation().Y, UnrealTransform.GetRotation().Z, UnrealTransform.GetRotation().W);
    physx::PxVec3 PxPos(UnrealTransform.GetTranslation().X, UnrealTransform.GetTranslation().Y, UnrealTransform.GetTranslation().Z);
    return physx::PxTransform(PxPos, PxRot);
}
