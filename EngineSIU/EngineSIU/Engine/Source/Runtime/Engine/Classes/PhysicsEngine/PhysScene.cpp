#include "PhysScene.h"
#include "PhysicsEngine/PhysicsInterfaceDeclaresCore.h"

FPhysScene::FPhysScene(physx::PxScene* InPxScene)
{
}

FPhysScene::~FPhysScene()
{
}

void FPhysScene::Simulate(float DeltaTime)
{
    ProcessDeferredPhysicsOperations();

    PxSceneInstance->simulate(DeltaTime);
    FetchResults(true);
    ProcessDeferredPhysicsOperations();
}

bool FPhysScene::FetchResults(bool Block)
{
    PxSceneInstance->fetchResults(Block);

    return false;
}

FPhysicsAggregateHandle FPhysScene::CreateAggregate(uint32 MaxActors, bool EnableSelfCollision)
{
    return FPhysicsAggregateHandle();
}

void FPhysScene::ReleaseAggregate(FPhysicsAggregateHandle AggregateHandle)
{
}

void FPhysScene::DeferPhysicsStateCreation(UPrimitiveComponent* Primitive)
{
}

void FPhysScene::RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Primitive)
{
}

void FPhysScene::ProcessDeferredPhysicsOperations()
{
    for (auto& obj : gObjects)
    {
        obj.UpdateFromPhysics();
    }
}
