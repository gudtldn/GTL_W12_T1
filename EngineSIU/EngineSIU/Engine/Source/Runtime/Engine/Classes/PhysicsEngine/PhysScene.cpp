#include "PhysScene.h"

//FPhysScene::FPhysScene(physx::PxScene* InPxScene)
//{
//}

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
    physx::PxPhysics* PxPhysicsSDK = gPhysics;
    if (PxSceneInstance && PxPhysicsSDK)
    {
        physx::PxAggregate* NewPxAgg = PxPhysicsSDK->createAggregate(MaxActors, EnableSelfCollision);
        if (NewPxAgg)
        {
            PxSceneInstance->addAggregate(*NewPxAgg);
            return FPhysicsAggregateHandle(NewPxAgg);
        }
    }
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
