#pragma once
#include "PhysicsEngine/PhysicsInterfaceDeclaresCore.h"
#include "UObject/ObjectMacros.h"

class UPrimitiveComponent;

class FPhysicsEngine
{
public:
    FPhysicsEngine();

    static void InitPhysX();
    static void ShutdownPhysX();

    static void Tick(float DeltaTime);

    static void StartSimulatePVD();
    static void EndSimulatePVD();

private:
    TArray<UPrimitiveComponent*> DeferredCreationQueue;
    TArray<UPrimitiveComponent*> DeferredDestructionQueue;

public:
    physx::PxScene* GetPxScene() const { return gScene; }

    bool IsValid() const { return gScene != nullptr; }

    static void Simulate(float DeltaTime);
    static bool FetchResults(bool Block = true);

    static FPhysicsAggregateHandle CreateAggregate(uint32 MaxActors, bool EnableSelfCollision = false);
    static void ReleaseAggregate(FPhysicsAggregateHandle AggregateHandle);

    void DeferPhysicsStateCreation(UPrimitiveComponent* Primitive);
    void RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Primitive);
    static void ProcessDeferredPhysicsOperations();
};

