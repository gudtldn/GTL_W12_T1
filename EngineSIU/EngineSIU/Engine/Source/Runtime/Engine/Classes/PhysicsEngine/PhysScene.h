// PhysicsInterfaceDeclaresCore.h

#pragma once

#include "Core/Container/Array.h"
#include "PhysicsEngine/PhysicsInterfaceDeclaresCore.h"

struct FPhysicsAggregateHandle;
class UPrimitiveComponent;

class FPhysScene
{
private:
    physx::PxScene* PxSceneInstance;

    TArray<UPrimitiveComponent*> DeferredCreationQueue;
    TArray<UPrimitiveComponent*> DeferredDestructionQueue;

public:
    explicit FPhysScene(physx::PxScene* InPxScene);
    ~FPhysScene();

    physx::PxScene* GetPxScene() const { return PxSceneInstance; }

    bool IsValid() const { return PxSceneInstance != nullptr; }

    // 래핑만 함
    // 전역으로 따로 빼기 vs FPhysScene에 그대로 두기
    void Simulate(float DeltaTime);
    bool FetchResults(bool Block = true);

    FPhysicsAggregateHandle CreateAggregate(uint32 MaxActors, bool EnableSelfCollision = false);
    void ReleaseAggregate(FPhysicsAggregateHandle AggregateHandle);
    
    // 아마 게임잼을 할 것 같은데 하게 되면 쓸 생각
    // bool Raycast(const FVector& Origin, const FVector& Direction, float MaxDistance, FHitResult& OutHit);

    void DeferPhysicsStateCreation(UPrimitiveComponent* Primitive);
    void RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Primitive);
    void ProcessDeferredPhysicsOperations(); // 매 틱 호출
};
