#pragma once

#include <PxPhysicsAPI.h>
#include <DirectXMath.h>
#include "Container/Array.h"
#include "UObject/ObjectMacros.h"

using namespace DirectX;
using namespace physx;

// Begin Test
// FIX-ME
extern physx::PxDefaultAllocator       gAllocator;
extern physx::PxDefaultErrorCallback   gErrorCallback;
extern physx::PxFoundation* gFoundation;
extern physx::PxPhysics* gPhysics;
extern physx::PxScene* gScene;
extern physx::PxMaterial* gMaterial;
extern physx::PxDefaultCpuDispatcher* gDispatcher;
// End Test


struct FGameObject 
{
    physx::PxRigidDynamic* rigidBody = nullptr;
    XMMATRIX worldMatrix = XMMatrixIdentity();
    void UpdateFromPhysics() {
        physx::PxTransform t = rigidBody->getGlobalPose();
        physx::PxMat44 mat(t);
        worldMatrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&mat));
    }
};

extern TArray<FGameObject> gObjects;


struct FPhysicsAggregateHandle
{
public:
    FPhysicsAggregateHandle() = default;

    explicit FPhysicsAggregateHandle(physx::PxAggregate* InAggregate) : Handle(InAggregate) {}

    physx::PxAggregate* GetUnderlyingAggregate() const { return Handle; }

    bool IsValid() const { return Handle != nullptr; }

public:
    void AddActor(physx::PxActor& Actor) const { if (Handle) Handle->addActor(Actor); }
    void RemoveActor(physx::PxActor& Actor) const { if (Handle)Handle->removeActor(Actor); }
    uint32 GetNumActors() const { if (Handle) return Handle->getNbActors(); }

    explicit operator bool() const { return IsValid(); }

    bool operator==(const FPhysicsAggregateHandle& Other) const { return Handle == Other.Handle; }
    bool operator!=(const FPhysicsAggregateHandle& Other) const { return Handle != Other.Handle; }

    void Reset() { Handle = nullptr; }

private:
    physx::PxAggregate* Handle;
};
