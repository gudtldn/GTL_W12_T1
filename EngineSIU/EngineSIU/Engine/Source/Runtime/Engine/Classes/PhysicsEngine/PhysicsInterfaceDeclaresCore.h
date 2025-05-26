#pragma once

#include <PxPhysicsAPI.h>
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

namespace physx
{
    class PxShape;
    class PxRigidDynamic;
    class PxRigidStatic;
    class PxJoint;
    class PxAggregate;
    class PxActor;
    class PxScene;
    class PxPhysics;
};

physx::PxDefaultAllocator       gAllocator;
physx::PxDefaultErrorCallback   gErrorCallback;
physx::PxFoundation*            gFoundation = nullptr;
physx::PxPhysics*               gPhysics = nullptr;
physx::PxScene*                 gScene = nullptr;
physx::PxMaterial*              gMaterial = nullptr; // 이거를 안썼네 
physx::PxDefaultCpuDispatcher*  gDispatcher = nullptr;

struct GameObject {
    physx::PxRigidDynamic* rigidBody = nullptr;
    XMMATRIX worldMatrix = XMMatrixIdentity();
    void UpdateFromPhysics() {
        physx::PxTransform t = rigidBody->getGlobalPose();
        physx::PxMat44 mat(t);
        worldMatrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&mat));
    }
};

TArray<GameObject> gObjects;

void InitPhysX() {
    gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
    gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, physx::PxTolerancesScale());
    gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.6f);

    physx::PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0, -9.81f, 0);
    gDispatcher = physx::PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = gDispatcher;
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    gScene = gPhysics->createScene(sceneDesc);
}

struct FPhysicsAggregateHandle
{
private:
    physx::PxAggregate* Handle;

public:
    FPhysicsAggregateHandle() : Handle(nullptr) {}
    explicit FPhysicsAggregateHandle(physx::PxAggregate* InAggregate) : Handle(InAggregate) {}

    physx::PxAggregate* GetUnderlyingAggregate() const { return Handle; }

    bool IsValid() const { return Handle != nullptr; }

    // void AddActor(physx::PxActor& Actor);
    // void RemoveActor(physx::PxActor& Actor);
    // uint32 GetNumActors() const;

    explicit operator bool() const { return IsValid(); }

    bool operator==(const FPhysicsAggregateHandle& Other) const { return Handle == Other.Handle; }
    bool operator!=(const FPhysicsAggregateHandle& Other) const { return Handle != Other.Handle; }

    void Reset() { Handle = nullptr; }
};
