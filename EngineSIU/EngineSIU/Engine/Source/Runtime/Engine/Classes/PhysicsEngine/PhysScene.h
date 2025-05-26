#pragma once

#include "Core/Container/Array.h"
#include "PhysicsEngine/PhysicsInterfaceDeclaresCore.h"
#include "UObject/ObjectMacros.h"

struct FPhysicsAggregateHandle;
class UPrimitiveComponent;

class FPhysScene
{
private:

    TArray<UPrimitiveComponent*> DeferredCreationQueue;
    TArray<UPrimitiveComponent*> DeferredDestructionQueue;

public:
    FPhysScene() = default;
    FPhysScene(physx::PxScene* InPxScene);

    physx::PxScene* GetPxScene() const { return gScene; }

    bool IsValid() const { return gScene != nullptr; }

    static void Simulate(float DeltaTime);
    static bool FetchResults(bool Block = true);

    FPhysicsAggregateHandle CreateAggregate(uint32 MaxActors, bool EnableSelfCollision = false);
    void ReleaseAggregate(FPhysicsAggregateHandle AggregateHandle);
    
    void DeferPhysicsStateCreation(UPrimitiveComponent* Primitive);
    void RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Primitive);
    static void ProcessDeferredPhysicsOperations();
};
