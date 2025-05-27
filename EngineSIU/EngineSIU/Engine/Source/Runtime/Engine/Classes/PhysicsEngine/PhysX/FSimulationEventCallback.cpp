#include "FSimulationEventCallback.h"


void FSimulationEventCallback::onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count)
{
}

void FSimulationEventCallback::onWake(physx::PxActor** actors, physx::PxU32 count)
{
}

void FSimulationEventCallback::onSleep(physx::PxActor** actors, physx::PxU32 count)
{
}

void FSimulationEventCallback::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
{
}

void FSimulationEventCallback::onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
{
}

void FSimulationEventCallback::onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count)
{
}
