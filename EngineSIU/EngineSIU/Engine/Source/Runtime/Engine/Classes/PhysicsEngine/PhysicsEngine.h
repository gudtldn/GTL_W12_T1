#pragma once
#include "PhysicsEngine/PhysicsInterfaceDeclaresCore.h"
#include "UObject/ObjectMacros.h"

class PhysScene;

class FPhysicsEngine
{
public:
    FPhysicsEngine();

    static void InitPhysX();
    static void ShutdownPhysX();

    static void Tick(float DeltaTime);

    static void StartSimulatePVD();
    static void EndSimulatePVD();
};

