#pragma once
#include "ecs_pch.h"
#include "ecs_type.h"
#include "query.h"

namespace ECS
{

template <typename T>
constexpr bool is_iterator_v = std::is_same_v<T, QueryIterator>;

template <typename T>
using decay_t =
    typename std::remove_const<typename std::remove_reference<T>::type>::type;

template <typename T, typename... Components>
struct is_in_component_list;

template <typename T>
struct is_in_component_list<T> : std::false_type
{
};

template <typename T, typename First, typename... Rest>
struct is_in_component_list<T, First, Rest...>
    : std::conditional_t<std::is_same_v<decay_t<T>, decay_t<First>>,
                         std::true_type, is_in_component_list<T, Rest...>>
{
};

template <typename T, typename... Components>
struct index_of;

template <typename T>
struct index_of<T>
{
    static constexpr uint32_t value = 0;
};

template <typename T, typename First, typename... Rest>
struct index_of<T, First, Rest...>
{
    static constexpr uint32_t value = std::is_same_v<decay_t<T>, decay_t<First>>
                                          ? 0
                                          : 1 + index_of<T, Rest...>::value;
};

template <typename T, typename... Components>
constexpr uint32_t index_of_v = index_of<T, Components...>::value;

struct SystemCallback
{
    SystemCallback() = default;

    SystemCallback(SystemCallback &&other) noexcept
    {
        funcPtr = other.funcPtr;
        invoker = other.invoker;
        archetypeList = other.archetypeList;
        componentSet = std::move(other.componentSet);
    }

    SystemCallback(const SystemCallback &other) = delete;

    SystemCallback &operator=(SystemCallback &&other) noexcept
    {
        funcPtr = other.funcPtr;
        invoker = other.invoker;
        archetypeList = other.archetypeList;
        componentSet = std::move(other.componentSet);

        return *this;
    }

    SystemCallback &operator=(const SystemCallback &other) = delete;

    void Execute(QueryIterator *it, void **componentsData)
    {
        invoker(funcPtr, it, componentsData);
    }

    void (*funcPtr)();
    void (*invoker)(void (*)(), QueryIterator *, void **);
    ArchetypeLinkedList *archetypeList;
    ComponentSet componentSet;
};

template <typename FuncArgs, typename... Components>
FuncArgs GetArgs(QueryIterator *it, void **componentsData)
{
    if constexpr (is_iterator_v<decay_t<FuncArgs>>)
    {
        if (std::is_const_v<FuncArgs>)
        {
            return *reinterpret_cast<const QueryIterator *>(it);
        }
        else
        {
            return *it;
        }
    }
    else
    {
        constexpr uint32_t idx = index_of_v<FuncArgs, Components...>;
        using ComponentType = decay_t<FuncArgs>;

        if constexpr (std::is_const_v<std::remove_reference_t<FuncArgs>>)
        {
            void *data = componentsData[idx];

            if (data == nullptr)
            {
                assert(-1 && "Component has no data!");
            }

            return *static_cast<const ComponentType *>(data);
        }
        else
        {
            void *data = componentsData[idx];

            if (data == nullptr)
            {
                assert(0 && "Component has no data!");
            }

            return *static_cast<ComponentType *>(data);
        }
    }
}

template <typename... Components, typename... FuncArgs>
SystemCallback CreateSystemCallback(void (*func)(FuncArgs...))
{
    static_assert(
        (... && (is_iterator_v<FuncArgs> ||
                 (is_in_component_list<FuncArgs, Components...>::value) &&
                     std::is_reference_v<FuncArgs>)),
        "Invalid system parameters!");

    SystemCallback cb;
    cb.funcPtr = RCAST(func, void (*)());
    cb.invoker = [](void (*fn)(), QueryIterator *it, void **componentsData)
    {
        auto actualFunc = RCAST(fn, void (*)(FuncArgs...));
        actualFunc(GetArgs<FuncArgs, Components...>(it, componentsData)...);
    };

    return cb;
}

} // namespace ECS
