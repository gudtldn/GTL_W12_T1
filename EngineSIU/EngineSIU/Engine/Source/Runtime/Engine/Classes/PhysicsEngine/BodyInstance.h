#pragma once
#include "BodyInstanceCore.h"

#include "PhysicsEngine/PhysicsInterfaceDeclaresCore.h"

class UPrimitiveComponent;
class UBodySetup;

struct FBodyInstance : FBodyInstanceCore
{
    DECLARE_STRUCT(FBodyInstance, FBodyInstanceCore)

public:
    FBodyInstance() = default;
    virtual ~FBodyInstance() override = default;

    FBodyInstance(const FBodyInstance&) = default;
    FBodyInstance& operator=(const FBodyInstance&) = default;
    FBodyInstance(FBodyInstance&&) = default;
    FBodyInstance& operator=(FBodyInstance&&) = default;

    bool InitBody(
        AActor* InOwningActor,
        UPrimitiveComponent* InOwningComponent,
        UBodySetup* InBodySetup,
        physx::PxScene* InScene,
        const FTransform& InInitialTransform,
        const FPhysicsAggregateHandle& InAggregate = FPhysicsAggregateHandle()
    );
     
    // Terminate
    void TermBody();

public:
    TWeakObjectPtr<UPrimitiveComponent> OwnerComponent;

    physx::PxRigidActor* RigidActor;

    bool IsInstanceKinematic() const;

    physx::PxTransform ConvertUnrealTransformToPx(const FTransform& UnrealTransform);

protected:
    bool bSimulatePhysics;
    bool bBodyInitialized;
};
