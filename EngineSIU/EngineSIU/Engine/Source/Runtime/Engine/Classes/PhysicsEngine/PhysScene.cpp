#include "PhysScene.h"

FPhysScene::FPhysScene(physx::PxScene* InPxScene)
{
    gScene = InPxScene;
    if (gScene)
    {
        //PxSceneInstance->setSimulationEventCallback(this);
    }
}

void FPhysScene::Simulate(float DeltaTime)
{
    gScene->simulate(DeltaTime);
    FetchResults(true);
    ProcessDeferredPhysicsOperations();
}

bool FPhysScene::FetchResults(bool Block)
{
    return gScene->fetchResults(Block);
}

FPhysicsAggregateHandle FPhysScene::CreateAggregate(uint32 MaxActors, bool EnableSelfCollision)
{
    physx::PxPhysics* PxPhysicsSDK = gPhysics;
    if (gScene && PxPhysicsSDK)
    {
        physx::PxAggregate* NewPxAgg = PxPhysicsSDK->createAggregate(MaxActors, EnableSelfCollision);
        if (NewPxAgg)
        {
            gScene->addAggregate(*NewPxAgg);
            return FPhysicsAggregateHandle(NewPxAgg);
        }
    }
    return FPhysicsAggregateHandle();
}

void FPhysScene::ReleaseAggregate(FPhysicsAggregateHandle AggregateHandle)
{
    if (gScene && AggregateHandle.IsValid())
    {
        physx::PxAggregate* PxAgg = AggregateHandle.GetUnderlyingAggregate();
        if (PxAgg)
        {
            gScene->removeAggregate(*PxAgg);
            PxAgg->release();
        }
    }
}

void FPhysScene::DeferPhysicsStateCreation(UPrimitiveComponent* Primitive)
{
    if (Primitive && !DeferredCreationQueue.Contains(Primitive))
    {
        DeferredCreationQueue.Add(Primitive);
    }
}

void FPhysScene::RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Primitive)
{
    if (Primitive)
    {
        DeferredCreationQueue.Remove(Primitive);
    }
}

void FPhysScene::ProcessDeferredPhysicsOperations()
{
    for (auto& obj : gObjects)
    {
        obj.UpdateFromPhysics();
    }
}
