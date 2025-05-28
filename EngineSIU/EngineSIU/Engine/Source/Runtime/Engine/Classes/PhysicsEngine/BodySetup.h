#pragma once
#include <PxFiltering.h>

#include "AggregateGeom.h"
#include "BodySetupCore.h"
#include "Engine/EngineTypes.h"


struct FCollisionResponseContainer
{
    DECLARE_STRUCT(FCollisionResponseContainer)

    // 각 채널에 대해 어떻게 반응할지 (Ignore, Overlap, Block)
    UPROPERTY(
        EditAnywhere | BitMask,
        ECollisionChannel, BlockMask, ;
    )

    UPROPERTY(
        EditAnywhere | BitMask,
        ECollisionChannel, OverlapMask, ; // 트리거/오버랩 이벤트용
    )

    UPROPERTY(
        EditAnywhere | BitMask,
        ECollisionChannel, HitMask, ; // onContact 이벤트용
    )

    // 기본적으로 모든 것과 블로킹
    FCollisionResponseContainer()
        : BlockMask(ECollisionChannel::All)
        , OverlapMask(static_cast<ECollisionChannel>(0))
        , HitMask(ECollisionChannel::All)
    {
    }
};

class UBodySetup : public UBodySetupCore
{
    DECLARE_CLASS(UBodySetup, UBodySetupCore)

public:
    UBodySetup() = default;
    virtual ~UBodySetup() override = default;

public:
    UPROPERTY(
        EditAnywhere, ({ .Category = "BodySetup", .DisplayName = "Primitives" }),
        FKAggregateGeom, AggGeom, ;
    )

    UPROPERTY(
        EditAnywhere | LuaReadWrite, ({ .Category = "Physics", .ToolTip = "Mass of the body for physics simulation", .ClampMin = 0.001f }),
        float, Mass, = 10.f;
    )

    UPROPERTY(
        EditAnywhere, { .Category = "Collision" },
        ECollisionChannel, ObjectType, = ECollisionChannel::WorldDynamic;
    )

    UPROPERTY(
        EditAnywhere, { .Category = "Collision" },
        FCollisionResponseContainer, CollisionResponses, ;
    )

    physx::PxFilterData CreateFilterData() const;
};
