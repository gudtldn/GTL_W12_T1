
#include "BodyInstance.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetupCore.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/PrimitiveComponent.h"


bool FBodyInstance::InitBody(AActor* InOwningActor, UPrimitiveComponent* InOwnerComponent, UBodySetup* InBodySetup, physx::PxScene* InScene, const FTransform& InInitialTransform, const FPhysicsAggregateHandle& InAggregate)
{
    if (RigidActor)
    {
        UE_LOG(ELogLevel::Warning, TEXT("FBodyInstance::InitBody: RigidActor already exists. Call TermBody first."));
        TermBody();
    }

    if (!InBodySetup || !InScene || !InOwnerComponent)
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
        physx::PxShape* NewShape = PxSDK->createShape(PxGeom, *gMaterial, true);
        if (NewShape)
        {
            NewShape->setLocalPose(PxShapeLocalPose);
            // 충돌 필터 데이터 설정 (BodySetup 또는 컴포넌트의 콜리전 설정 기반)
            //NewShape->setSimulationFilterData(...);
            //NewShape->setQueryFilterData(...);
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
        if (BodySetup->bOverrideMass && BodySetup->Mass > 0.0f)
        {
            DynActor->setMass(BodySetup->Mass);
            physx::PxTransform MassFrame(physx::PxIdentity);
            physx::PxVec3 InertiaTensorDiagonal(1.f, 1.f, 1.f);
            if (RigidActor->getNbShapes() > 0)
            {
                PxRigidBodyExt::updateMassAndInertia(*DynActor, BodySetup->Density > 0.f ? BodySetup->Density : 1.0f);
            }
            else
            {
                DynActor->setMassSpaceInertiaTensor(InertiaTensorDiagonal);
            }

        }
        else if (BodySetup->Density > 0.0f && RigidActor->getNbShapes() > 0)
        {
            if (!PxRigidBodyExt::updateMassAndInertia(*DynActor, BodySetup->Density))
            {
                UE_LOG(ELogLevel::Warning, TEXT("FBodyInstance::InitBody: Failed to update mass and inertia from density. Using default values."));
                DynActor->setMass(1.0f);
                DynActor->setMassSpaceInertiaTensor(physx::PxVec3(1.0f, 1.0f, 1.0f));
            }
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("FBodyInstance::InitBody: Mass or Density not specified. Using default mass and inertia."));
            DynActor->setMass(1.0f);
            DynActor->setMassSpaceInertiaTensor(physx::PxVec3(1.0f, 1.0f, 1.0f));
        }

        DynActor->setLinearDamping(BodySetup->LinearDamping);
        DynActor->setAngularDamping(BodySetup->AngularDamping);

    }


    if (InAggregate.IsValid())
    {
        physx::PxAggregate* PxAgg = InAggregate.GetUnderlyingAggregate();
        if (PxAgg)
        {
            PxAgg->addActor(*RigidActor);
        }
    }

    InScene->addActor(*RigidActor);
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
    if (RigidActor && RigidActor->getConcreteType() == physx::PxConcreteType::eRIGID_DYNAMIC)
    {
        physx::PxRigidDynamic* DynActor = static_cast<physx::PxRigidDynamic*>(RigidActor);
        return (DynActor->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC);
    }
    return false;
}

physx::PxTransform FBodyInstance::ConvertUnrealTransformToPx(const FTransform& UnrealTransform)
{
    physx::PxQuat PxRot(UnrealTransform.GetRotation().X, UnrealTransform.GetRotation().Y, UnrealTransform.GetRotation().Z, UnrealTransform.GetRotation().W);
    physx::PxVec3 PxPos(UnrealTransform.GetTranslation().X, UnrealTransform.GetTranslation().Y, UnrealTransform.GetTranslation().Z);
    return physx::PxTransform(PxPos, PxRot);
}
