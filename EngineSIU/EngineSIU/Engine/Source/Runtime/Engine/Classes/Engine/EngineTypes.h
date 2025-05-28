#pragma once
#include "Core/HAL/PlatformType.h"
#include "magic_enum/magic_enum.hpp"

namespace EEndPlayReason
{
    enum Type : uint8
    {
        /** 명시적인 삭제가 일어났을 때, Destroy()등 */
        Destroyed,
        /** World가 바뀌었을 때 */
        WorldTransition,
        /** 프로그램을 종료했을 때 */
        Quit,
    };
}

enum class ECollisionChannel : uint32
{
    WorldStatic  = 1 << 0,
    WorldDynamic = 1 << 1,
    Pawn         = 1 << 2,
    Visibility   = 1 << 3,
    Camera       = 1 << 4,
    PhysicsBody  = 1 << 5,
    Vehicle      = 1 << 6,
    All          = 0xFFFFFFFF
};

// magic_enum에서 ECollisionChannel를 사용하기 위함
template <>
struct magic_enum::customize::enum_range<ECollisionChannel>
{
    static constexpr bool is_flags = true;
};
