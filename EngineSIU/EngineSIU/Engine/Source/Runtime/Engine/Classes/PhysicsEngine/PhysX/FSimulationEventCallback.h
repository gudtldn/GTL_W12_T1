#pragma once
#include <PxSimulationEventCallback.h>


struct FSimulationEventCallback : public physx::PxSimulationEventCallback
{
public:
    FSimulationEventCallback() = default;

public:
    virtual void onConstraintBreak(physx::PxConstraintInfo* Constraints, physx::PxU32 Count) override;
    virtual void onWake(physx::PxActor** Actors, physx::PxU32 Count) override;
    virtual void onSleep(physx::PxActor** Actors, physx::PxU32 Count) override;
    virtual void onContact(const physx::PxContactPairHeader& PairHeader, const physx::PxContactPair* Pairs, physx::PxU32 NbPairs) override;
    virtual void onTrigger(physx::PxTriggerPair* Pairs, physx::PxU32 Count) override;
    virtual void onAdvance(const physx::PxRigidBody* const* BodyBuffer, const physx::PxTransform* PoseBuffer, const physx::PxU32 Count) override;
};
