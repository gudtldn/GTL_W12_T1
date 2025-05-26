#include "PhysicsEngine.h"
#include "PhysicsEngine/PhysScene.h"

physx::PxDefaultAllocator       gAllocator;
physx::PxDefaultErrorCallback   gErrorCallback;
physx::PxFoundation* gFoundation = nullptr;
physx::PxPhysics* gPhysics = nullptr;
physx::PxScene* gScene = nullptr;
physx::PxMaterial* gMaterial = nullptr;
physx::PxDefaultCpuDispatcher* gDispatcher = nullptr;

TArray<FGameObject> gObjects;

void FPhysicsEngine::InitPhysX()
{
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

void FPhysicsEngine::ShutdownPhysX()
{
    if (gScene)
    {
        gScene->release();
        gScene = nullptr;
    }
    if (gDispatcher)
    {
        gDispatcher->release();
        gDispatcher = nullptr;
    }
    if (gMaterial)
    {
        gMaterial->release();
        gMaterial = nullptr;
    }
    if (gPhysics)
    {
        gPhysics->release();
        gPhysics = nullptr;
    }
    if (gFoundation)
    {
        gFoundation->release();
        gFoundation = nullptr;
    }
}
void FPhysicsEngine::Tick(float DeltaTime)
{

}
