#pragma once
#include "BodyInstanceCore.h"

class UBodySetup;
class UPrimitiveComponent;


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

    void SetUserData(void* InUserData);
    void* GetUserData() const;

private:
    TWeakObjectPtr<UPrimitiveComponent> OwnerComponent;

    struct FBodyInstancePImpl;
    std::unique_ptr<FBodyInstancePImpl> PImpl;
};
