#pragma once

struct FPhysX
{
    static void Initialize();
    static void Tick(float DeltaTime);
    static void Release();

    static void StartSimulatePVD();
    static void EndSimulatePVD();
};
