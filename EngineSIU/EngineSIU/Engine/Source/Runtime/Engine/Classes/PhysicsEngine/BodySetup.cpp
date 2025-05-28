#include "BodySetup.h"


physx::PxFilterData UBodySetup::CreateFilterData() const
{
    physx::PxFilterData NewFilterData;

    // word0: 이 객체의 타입 (하나의 비트만 설정하는 것이 일반적)
    NewFilterData.word0 = static_cast<physx::PxU32>(ObjectType);

    // word1: 이 객체가 블로킹 충돌할 대상 마스크
    NewFilterData.word1 = static_cast<physx::PxU32>(CollisionResponses.BlockMask);

    // word2: 이 객체가 오버랩/터치 이벤트를 발생시킬 대상 마스크 (또는 이벤트 발생 여부 플래그)
    // 여기서는 간단히 터치(onContact) 이벤트를 위한 마스크를 word2에 저장한다고 가정.
    // 또는 word2의 특정 비트를 사용하여 "히트 이벤트 생성" 플래그를 저장할 수도 있습니다.
    NewFilterData.word2 = static_cast<physx::PxU32>(CollisionResponses.HitMask);

    // word3: 추가 데이터 (여기서는 사용 안 함)
    NewFilterData.word3 = 0;

    return NewFilterData;
}
