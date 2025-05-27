#include "CarActor.h"
#include "Components/CubeComp.h"
#include "Components/SphereComp.h"
#include "Engine/FObjLoader.h"

ACarActor::ACarActor()
{
    FVector AABBMin = FVector(-5.f, -3.f, -2.f);
    FVector AABBMax = FVector(5.f, 3.f, 2.f);

    FObjManager::CreateStaticMesh(TEXT("Contents/Primitives/CubePrimitive.obj"));
    FObjManager::CreateStaticMesh(TEXT("Contents/Primitives/SpherePrimitive.obj"));

    Body = AddComponent<UCubeComp>(TEXT("BodyCube"));
    SetRootComponent(Body);
    Body->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Primitives/CubePrimitive.obj"));

    Body->AABB.MinLocation = AABBMin;
    Body->AABB.MaxLocation = AABBMax;
    Body->SetWorldScale3D(AABBMax - AABBMin);

    float WheelRadius = 0.7f; 
    FVector WheelOffsets[4] = {
        FVector(AABBMax.X, AABBMax.Y, AABBMin.Z - WheelRadius), // 앞 오른쪽
        FVector(AABBMax.X, AABBMin.Y, AABBMin.Z - WheelRadius), // 앞 왼쪽
        FVector(AABBMin.X, AABBMax.Y, AABBMin.Z - WheelRadius), // 뒤 오른쪽
        FVector(AABBMin.X, AABBMin.Y, AABBMin.Z - WheelRadius)  // 뒤 왼쪽
    };


    for (int i = 0; i < 4; ++i)
    {
        FString WheelName = FString::Printf(TEXT("Wheel0%d"), i);
        Wheels[i] = AddComponent<USphereComp>(*WheelName);
        Wheels[i]->SetWorldScale3D(FVector(WheelRadius));
        Wheels[i]->SetupAttachment(Body);
        Wheels[i]->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Primitives/SpherePrimitive.obj"));

        // 바퀴 위치를 AABB 기준 네 귀퉁이로 설정
        Wheels[i]->SetRelativeLocation(WheelOffsets[i]);
    }
}
