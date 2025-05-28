#include "FSimulationEventCallback.h"

#include <PxRigidActor.h>

#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Math/Vector.h"
#include "PhysicsEngine/BodyInstance.h"


void FSimulationEventCallback::onConstraintBreak(physx::PxConstraintInfo* Constraints, physx::PxU32 Count)
{
}

void FSimulationEventCallback::onWake(physx::PxActor** Actors, physx::PxU32 Count)
{
}

void FSimulationEventCallback::onSleep(physx::PxActor** Actors, physx::PxU32 Count)
{
}

void FSimulationEventCallback::onContact(const physx::PxContactPairHeader& PairHeader, const physx::PxContactPair* Pairs, physx::PxU32 NbPairs)
{
    // 1. 유효성 검사 및 기본 정보 추출
    if (PairHeader.flags & (physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
    {
        return; // 제거된 액터에 대한 이벤트는 무시
    }

    physx::PxRigidActor* RigidActorA_PhysX = PairHeader.actors[0];
    physx::PxRigidActor* RigidActorB_PhysX = PairHeader.actors[1];

    FBodyInstance* BodyInstA = RigidActorA_PhysX ? static_cast<FBodyInstance*>(RigidActorA_PhysX->userData) : nullptr;
    FBodyInstance* BodyInstB = RigidActorB_PhysX ? static_cast<FBodyInstance*>(RigidActorB_PhysX->userData) : nullptr;

    if (!BodyInstA || !BodyInstB)
    {
        // UE_LOG(LogPhysics, Warning, TEXT("onContact: BodyInstance userData is null for one or both actors."));
        return;
    }

    UPrimitiveComponent* CompA = BodyInstA->GetOwnerComponent();
    UPrimitiveComponent* CompB = BodyInstB->GetOwnerComponent();

    if (!CompA || !CompB)
    {
        // UE_LOG(LogPhysics, Warning, TEXT("onContact: OwnerComponent is null for one or both BodyInstances."));
        return;
    }

    AActor* ActorA = CompA->GetOwner();
    AActor* ActorB = CompB->GetOwner();

    if (!ActorA || !ActorB)
    {
        // UE_LOG(LogPhysics, Warning, TEXT("onContact: Owner Actor is null for one or both Components."));
        return; // 액터가 없는 컴포넌트의 충돌은 일반적이지 않으므로 로그 후 반환 고려
    }

    // 2. 이벤트 생성 조건 확인 (각 컴포넌트가 Hit 이벤트를 생성하도록 설정되었는지)
    bool bShouldGenerateHitEventsA = CompA->GetGenerateHitEvents();
    bool bShouldGenerateHitEventsB = CompB->GetGenerateHitEvents();

    if (!bShouldGenerateHitEventsA && !bShouldGenerateHitEventsB)
    {
        // 양쪽 모두 히트 이벤트를 생성하지 않도록 설정되어 있으면 반환
        return;
    }

    // 3. 각 ContactPair 처리
    for (physx::PxU32 i = 0; i < NbPairs; ++i)
    {
        const physx::PxContactPair& ContactPair = Pairs[i];

        if (ContactPair.flags & (physx::PxContactPairFlag::eREMOVED_SHAPE_0 | physx::PxContactPairFlag::eREMOVED_SHAPE_1))
        {
            continue; // 제거된 Shape에 대한 이벤트는 무시
        }

        // "터치 시작" 이벤트만 처리 (가장 일반적인 경우)
        // 필요에 따라 eNOTIFY_TOUCH_PERSISTS (계속 접촉 중), eNOTIFY_TOUCH_LOST (접촉 종료) 등도 처리 가능
        if (ContactPair.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            // 3.1. FHitResult 구성
            // 하나의 물리적 충돌에 대해, 각 액터/컴포넌트 관점에서 FHitResult를 생성합니다.
            FHitResult HitResultForA; // ActorA가 ActorB와 충돌한 결과
            FHitResult HitResultForB; // ActorB가 ActorA와 충돌한 결과

            // 3.2. 상세 접촉점 정보 추출
            TArray<physx::PxContactPairPoint> ContactPoints; // PxContactPoint -> PxContactPairPoint 로 수정!
            if (ContactPair.contactCount > 0)
            {
                ContactPoints.SetNum(ContactPair.contactCount);
                ContactPair.extractContacts(
                    ContactPoints.GetData(),
                    ContactPair.contactCount
                );

                const physx::PxContactPairPoint& FirstContactPoint = ContactPoints[0]; // 편의상 첫 번째 접촉점 사용

                // --- HitResultForA (CompA가 CompB와 충돌) ---
                HitResultForA.bBlockingHit = true;                     // 물리적 충돌은 블로킹으로 간주
                HitResultForA.Time = 0.0f;                             // 즉시 발생한 충돌
                HitResultForA.Distance = FirstContactPoint.separation; // 분리 거리 (음수면 관통)

                HitResultForA.ImpactPoint = FVector(FirstContactPoint.position.x, FirstContactPoint.position.y, FirstContactPoint.position.z);
                HitResultForA.ImpactNormal = FVector(
                    FirstContactPoint.normal.x, FirstContactPoint.normal.y, FirstContactPoint.normal.z
                );                                                  // CompA에 대한 법선
                HitResultForA.Location = HitResultForA.ImpactPoint; // 충돌 위치
                HitResultForA.Normal = HitResultForA.ImpactNormal;  // 충돌 법선

                // HitResultForA.Actor = ActorB;    // 충돌한 상대 액터
                HitResultForA.Component = CompB; // 충돌한 상대 컴포넌트
                // HitResultForA.BoneName = GetBoneNameFromShape(ContactPair.shapes[1]); // 상대방 Shape에 연결된 본 이름 (구현 필요)
                // HitResultForA.PhysMaterial = BodyInstB->GetPhysicalMaterial();        // 상대방 물리 머티리얼 (구현 필요)

                // --- HitResultForB (CompB가 CompA와 충돌) ---
                // 기본 정보는 HitResultForA와 유사하지만, 관점이 다름
                HitResultForB.bBlockingHit = true;
                HitResultForB.Time = 0.0f;
                HitResultForB.Distance = FirstContactPoint.separation; // 동일한 분리 거리

                HitResultForB.ImpactPoint = HitResultForA.ImpactPoint;    // 충돌 지점은 동일
                HitResultForB.ImpactNormal = -HitResultForA.ImpactNormal; // 법선 방향은 반대
                HitResultForB.Location = HitResultForB.ImpactPoint;
                HitResultForB.Normal = HitResultForB.ImpactNormal;

                // HitResultForB.Actor = ActorA;
                HitResultForB.Component = CompA;
                // HitResultForB.BoneName = GetBoneNameFromShape(ContactPair.shapes[0]); // 자신의 Shape에 연결된 본 이름
                // HitResultForB.PhysMaterial = BodyInstA->GetPhysicalMaterial();

                // 3.3. 델리게이트 호출
                // 각 액터/컴포넌트가 이벤트를 받도록 설정되어 있을 때만 호출

                // CompA에 대한 이벤트
                if (bShouldGenerateHitEventsA)
                {
                    // 사용자 엔진의 FComponentHitSignature에 맞게 파라미터 전달
                    // 예시: CompA->OnComponentHit.Broadcast(MyComp, OtherActor, OtherComp, Normal, HitResult);
                    // 여기서는 CompA, ActorB (상대 액터), CompB (상대 컴포넌트), HitResultForA.Normal, HitResultForA 를 전달한다고 가정
                    CompA->OnComponentHit.Broadcast(CompA, ActorB, CompB, HitResultForA.Normal, HitResultForA);
                    if (ActorA) // ActorA가 null이 아닐 때만 (위에서 이미 체크했지만, 안전하게)
                    {
                        // 사용자 엔진의 FActorHitSignature에 맞게 파라미터 전달
                        // 예시: ActorA->OnActorHit.Broadcast(SelfActor, OtherActor, NormalImpulse, Hit);
                        // 여기서는 ActorA, ActorB, CompA (자신의 컴포넌트), HitResultForA.ImpactPoint, HitResultForA.Normal 을 전달한다고 가정
                        ActorA->OnActorHit.Broadcast(ActorA, ActorB, HitResultForA.ImpactPoint, HitResultForA);
                    }
                }

                // CompB에 대한 이벤트
                if (bShouldGenerateHitEventsB)
                {
                    CompB->OnComponentHit.Broadcast(CompB, ActorA, CompA, HitResultForB.Normal, HitResultForB);
                    if (ActorB)
                    {
                        ActorB->OnActorHit.Broadcast(ActorB, ActorA, HitResultForB.ImpactPoint, HitResultForB);
                    }
                }
            }
        }
    }
}

void FSimulationEventCallback::onTrigger(physx::PxTriggerPair* Pairs, physx::PxU32 Count)
{
}

void FSimulationEventCallback::onAdvance(const physx::PxRigidBody* const* BodyBuffer, const physx::PxTransform* PoseBuffer, const physx::PxU32 Count)
{
}
