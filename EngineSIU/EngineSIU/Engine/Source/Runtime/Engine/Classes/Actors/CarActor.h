#pragma once
#include "GameFramework/Actor.h"

class UCubeComp;
class USphereComp;

class ACarActor : public AActor
{
    DECLARE_CLASS(ACarActor, AActor);

public:
    ACarActor();
    virtual ~ACarActor() = default;


private:
    UCubeComp* Body = nullptr;
    USphereComp* Wheels[4] = { nullptr, nullptr, nullptr, nullptr };

};

