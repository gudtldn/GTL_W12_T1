#pragma once
#include "BodyInstanceCore.h"

#include "PxPhysicsAPI.h" 

class UBodySetup;
class UPrimitiveComponent;

struct FKSphereElem; 
struct FKBoxElem;
struct FKSphylElem;
struct FKConvexElem;

struct FBodyInstance : FBodyInstanceCore
{
    DECLARE_STRUCT(FBodyInstance, FBodyInstanceCore)

public:
    FBodyInstance();
    virtual ~FBodyInstance() override;

    FBodyInstance(const FBodyInstance&) = delete;
    FBodyInstance& operator=(const FBodyInstance&) = delete;
    FBodyInstance(FBodyInstance&&) noexcept;
    FBodyInstance& operator=(FBodyInstance&&) noexcept;

public:
    UPrimitiveComponent* GetOwnerComponent() const { return OwnerComponent; }

    void InitBody(UPrimitiveComponent* InOwnerComponent, const UBodySetup* InBodySetup, const FTransform& InTransform, bool bInSimulatePhysics);
    void TermBody();

    void SyncPhysXToComponent();
    void SyncComponentToPhysX();

    bool IsValidBodyInstance() const;

    void AddForce(const FVector& Force, bool bAccelChange);
    void SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent);
    FVector GetLinearVelocity() const;
    void SetAngularVelocity(const FVector& NewAngVel, bool bAddToCurrent);
    FVector GetAngularVelocity() const;
    FTransform GetWorldTransform() const;
    void SetWorldTransform(const FTransform& NewTransform, bool bTeleportPhysics);
    void SetSimulatePhysics(bool bSimulate);
    bool IsSimulatingPhysics() const;

private:
    TWeakObjectPtr<UPrimitiveComponent> OwnerComponent;

private:
    // UBodySetup의 FKAggregateGeom 정보를 바탕으로 PxShape들을 생성하고 PxRigidActor에 붙입니다.
    void CreateShapesFromAggGeom(const UBodySetup* BodySetupRef, physx::PxRigidActor* OutActor);

    physx::PxShape* CreateShapeFromSphere(const FKSphereElem& SphereElem, const physx::PxMaterial& Material) const;
    physx::PxShape* CreateShapeFromBox(const FKBoxElem& BoxElem, const physx::PxMaterial& Material) const;
    physx::PxShape* CreateShapeFromSphyl(const FKSphylElem& SphylElem, const physx::PxMaterial& Material) const;
    physx::PxShape* CreateShapeFromConvex(const FKConvexElem& ConvexElem, const physx::PxMaterial& Material) const;

    physx::PxRigidActor* RigidActor;

    // 현재 물리 시뮬레이션 여부
    bool bIsSimulatingPhysics;
};
