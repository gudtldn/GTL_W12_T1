#pragma once
#include <utility>
#include "HAL/PlatformType.h"

class UObject;


template <typename Signature>
struct TFunction;

template <typename ReturnType, typename... ParamsType>
struct TFunction<ReturnType(ParamsType...)>
{
private:
    // 내부 저장소 및 호출을 위한 인터페이스
    struct ICallable
    {
        ICallable() = default;
        virtual ~ICallable() = default;

        ICallable(const ICallable&) = default;
        ICallable& operator=(const ICallable&) = default;
        ICallable(ICallable&&) = default;
        ICallable& operator=(ICallable&&) = default;

        virtual ReturnType Invoke(ParamsType...) = 0;
        [[nodiscard]] virtual ICallable* Clone() const = 0;
    };

    // 함수 포인터를 저장하는 Callable
    template <typename FuncPtrType>
    struct TFunctionPtrCallable final : ICallable
    {
        FuncPtrType Func;
        explicit TFunctionPtrCallable(FuncPtrType InFunc) : Func(std::move(InFunc)) {}

        virtual ReturnType Invoke(ParamsType... Args) override
        {
            return Func(std::forward<ParamsType>(Args)...);
        }

        virtual ICallable* Clone() const override
        {
            return new TFunctionPtrCallable(Func);
        }
    };

    // 멤버 함수 포인터를 저장하는 Callable
    template <typename ClassType, typename MemberFuncPtrType>
    struct TMemberFunctionPtrCallable final : ICallable
    {
        ClassType* Object;
        MemberFuncPtrType MemberFunc;

        TMemberFunctionPtrCallable(ClassType* InObject, MemberFuncPtrType InMemberFunc)
            : Object(InObject), MemberFunc(InMemberFunc) {}

        virtual ReturnType Invoke(ParamsType... Args) override
        {
            return (Object->*MemberFunc)(std::forward<ParamsType>(Args)...);
        }

        virtual ICallable* Clone() const override
        {
            return new TMemberFunctionPtrCallable(Object, MemberFunc);
        }
    };

    // 람다 및 기타 Callable한 객체를 저장하는 Callable
    template <typename FunctorType>
    struct TFunctorCallable final : ICallable
    {
        FunctorType Functor;

        explicit TFunctorCallable(const FunctorType& InFunctor) : Functor(InFunctor) {}
        explicit TFunctorCallable(FunctorType&& InFunctor) : Functor(std::move(InFunctor)) {}

        virtual ReturnType Invoke(ParamsType... Args) override
        {
            return Functor(std::forward<ParamsType>(Args)...);
        }

        virtual ICallable* Clone() const override
        {
            return new TFunctorCallable(Functor);
        }
    };

    // TFunction이 가지고 있는 Callable 객체
    ICallable* Storage = nullptr;

public:
    TFunction() = default;
    TFunction(nullptr_t) : Storage(nullptr) {}

    ~TFunction()
    {
        Reset();
    }

    // 복사 생성자
    TFunction(const TFunction& Other)
    {
        if (Other.Storage)
        {
            Storage = Other.Storage->Clone();
        }
    }

    // 복사 대입 연산자
    TFunction& operator=(const TFunction& Other)
    {
        if (this != &Other)
        {
            delete Storage;
            if (Other.Storage)
            {
                Storage = Other.Storage->Clone();
            }
            else
            {
                Storage = nullptr;
            }
        }
        return *this;
    }

    // nullptr 대입 연산자
    TFunction& operator=(nullptr_t)
    {
        Reset();
        return *this;
    }

    // 이동 생성자
    TFunction(TFunction&& Other) noexcept
    {
        Storage = Other.Storage;
        Other.Storage = nullptr;
    }

    // 이동 대입 연산자
    TFunction& operator=(TFunction&& Other) noexcept
    {
        if (this != &Other)
        {
            delete Storage;
            Storage = Other.Storage;
            Other.Storage = nullptr;
        }
        return *this;
    }

    // 일반 함수 포인터를 받는 생성자
    template <typename FuncPtrType>
    requires
        std::is_pointer_v<FuncPtrType>
        && std::is_function_v<std::remove_pointer_t<FuncPtrType>>
        && std::is_invocable_r_v<ReturnType, FuncPtrType, ParamsType...>
    TFunction(FuncPtrType InFunc)
    {
        // nullptr 함수 포인터 검사
        if (InFunc)
        {
            Storage = new TFunctionPtrCallable<FuncPtrType>(InFunc);
        }
    }

    // 멤버 함수 포인터와 객체 포인터를 받는 생성자
    template <typename ClassType, typename MemberFuncPtrType>
    requires
        std::is_class_v<ClassType>
        && std::is_member_function_pointer_v<MemberFuncPtrType>
        && std::is_invocable_r_v<ReturnType, MemberFuncPtrType, ClassType*, ParamsType...>
    TFunction(ClassType* InObject, MemberFuncPtrType InMemberFunc)
    {
        if constexpr (std::derived_from<ClassType, UObject>)
        {
            if (!IsValid(InObject))
            {
                return;
            }
        }
        else
        {
            if (!InObject)
            {
                return;
            }
        }

        if (InMemberFunc)
        {
            Storage = new TMemberFunctionPtrCallable<ClassType, MemberFuncPtrType>(InObject, InMemberFunc);
        }
    }

    // 람다 및 기타 Callable한 객체를 받는 생성자
    template <typename FunctorType>
    requires
        (!std::same_as<std::decay_t<FunctorType>, TFunction>) // 자기 자신은 제외
        && !( // 일반 함수 포인터는 위에서 처리
            std::is_pointer_v<std::decay_t<FunctorType>>
            && std::is_function_v<std::remove_pointer_t<std::decay_t<FunctorType>>>
        )
        && (!std::is_member_function_pointer_v<std::decay_t<FunctorType>>)              // 멤버 함수 포인터 단독은 무시
        && (!std::is_null_pointer_v<std::decay_t<FunctorType>>)                         // nullptr_t는 별도 생성자에서 처리
        && std::is_invocable_r_v<ReturnType, std::decay_t<FunctorType>&, ParamsType...> // 호출 가능성 검사 (반환 타입 포함)
        && std::is_copy_constructible_v<std::decay_t<FunctorType>>                      // 복사 생성 가능한지 여부, Clone 때문에
    TFunction(FunctorType&& InFunctor)
    {
        using DecayedFunctorType = std::decay_t<FunctorType>;
        Storage = new TFunctorCallable<DecayedFunctorType>(std::forward<FunctorType>(InFunctor));
    }

    FORCEINLINE explicit operator bool() const noexcept
    {
        return IsBound();
    }

    FORCEINLINE bool operator==(std::nullptr_t) const
    {
        return !IsBound();
    }

    FORCEINLINE bool operator!=(std::nullptr_t) const
    {
        return IsBound();
    }

    ReturnType operator()(ParamsType... Args) const
    {
        if (IsBound())
        {
            return Storage->Invoke(std::forward<ParamsType>(Args)...);
        }
        return ReturnType{};
    }

public:
    /** TFunction이 유효한 객체을 가리키고 있는지 확인합니다. */
    [[nodiscard]] bool IsBound() const noexcept
    {
        return Storage != nullptr;
    }

    /** 저장된 Callable 객체를 초기화합니다. */
    void Reset() noexcept
    {
        delete Storage;
        Storage = nullptr;
    }
};
