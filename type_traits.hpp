#pragma once
#include <type_traits>
#include <tuple>
#include <variant>

#include "type_algorithms.hpp"
#include "metamodel.hpp"

namespace metahsm {

template <typename _StateDef>
struct state_spec
{
    using type = _StateDef;
};

template <typename _StateDef>
using state_spec_t = typename state_spec<_StateDef>::type;

template <typename _StateDef>
using meta_type_t = typename MetaType<_StateDef>::Type;


template <typename _StateDef, typename _Event, typename _Enable = void>
struct has_reaction_to_event : public std::false_type
{};

template <typename _StateDef, typename _Event>
struct has_reaction_to_event<_StateDef, _Event, std::void_t<std::is_invocable<decltype(&_StateDef::react), _StateDef&, const _Event&>>>
: std::true_type
{};

template <typename _StateDef, typename _Event>
constexpr bool has_reaction_to_event_v = has_reaction_to_event<_StateDef, _Event>::value;

template <typename _StateDef, typename _ContextDef, typename Enable = void>
struct is_in_context_recursive : std::false_type
{};

template <typename _StateDef>
struct is_in_context_recursive<_StateDef, _StateDef, void> : std::true_type
{};

template <typename _StateDef, typename ... _ContextDef>
struct is_in_context_recursive<_StateDef, std::tuple<_ContextDef...>> :
std::disjunction<is_in_context_recursive<_StateDef, _ContextDef>...>
{};

template <typename _StateDef, typename _ContextDef>
struct is_in_context_recursive<_StateDef, _ContextDef, std::enable_if_t<
        !std::is_same_v<_StateDef, _ContextDef> &&
        !std::is_void_v<typename _ContextDef::SubStates>
    >>
: is_in_context_recursive<_StateDef, typename _ContextDef::SubStates>
{};

template <typename _StateDef, typename _ContextDef>
constexpr bool is_in_context_recursive_v = is_in_context_recursive<_StateDef, _ContextDef>::value;

template <typename _StateDef1, typename _StateDef2, typename _ContextDef, typename _Enable = void>
struct lca
{
    using type = void;
};

template <typename _StateDef, typename _ContextDef>
struct lca<_StateDef, _StateDef, _ContextDef, void>
{
    using type = _StateDef;
};

template <typename _StateDef1, typename _StateDef2, typename ... _ContextDef>
struct lca<_StateDef1, _StateDef2, std::tuple<_ContextDef...>, void>
{
    using type = typename collapse<typename lca<_StateDef1, _StateDef2, _ContextDef>::type...>::type;
};

template <typename _StateDef1, typename _StateDef2, typename _ContextDef>
struct lca<_StateDef1, _StateDef2, _ContextDef, std::enable_if_t<
        !std::is_void_v<typename _ContextDef::SubStates> &&
        !std::is_same_v<_StateDef1, _StateDef2> &&
        is_in_context_recursive_v<_StateDef1, _ContextDef> &&
        is_in_context_recursive_v<_StateDef2, _ContextDef>
    >>
{
    using SubType = typename lca<_StateDef1, _StateDef2, typename _ContextDef::SubStates>::type;
    using type = typename collapse<SubType, _ContextDef>::type;
};

template <typename _StateDef1, typename _StateDef2>
using lca_t = typename lca<_StateDef1, _StateDef2, typename _StateDef1::TopStateDef>::type;

template <typename _StateDef, typename _StateDefToCompare, typename _ContextDef, typename Enable = void>
struct context
{
    using type = void;
};

template <typename _StateDef, typename ... _StateDefToCompare, typename _ContextDef>
struct context<_StateDef, std::tuple<_StateDefToCompare...>, _ContextDef, void>
{
    using type = typename collapse<typename context<_StateDef, _StateDefToCompare, _ContextDef>::type...>::type;
};

template <typename _StateDef, typename _StateDefToCompare, typename _ContextDef>
struct context<_StateDef, _StateDefToCompare, typename _ContextDef, std::enable_if_t<
        !std::is_void_v<typename _StateDefToCompare::SubStates> &&
        !std::is_same_v<_StateDef, _StateDefToCompare> &&
        is_in_context_recursive_v<_StateDef, _StateDefToCompare>
    >>
{
    using SubType = typename context<_StateDef, typename _StateDefToCompare::SubStates, _StateDefToCompare>::type;
    using type = typename collapse<SubType, _StateDefToCompare>::type;
};

template <typename _StateDef, typename _ContextDef>
struct context<_StateDef, _StateDef, _ContextDef, void>
{
    using type = _ContextDef;
};

template <typename _StateDef>
using context_t  = typename context<_StateDef, typename _StateDef::TopStateDef, typename _StateDef::TopStateDef>::type;


template <typename _StateDef, typename _SFINAE = void>
struct is_initial_specified : std::false_type {};

template <typename _StateDef>
struct is_initial_specified<_StateDef, std::void_t<typename _StateDef::Initial>> : std::true_type {};

template <typename _StateDef>
constexpr bool is_initial_specified_v = is_initial_specified<_StateDef>::value;

template <typename _StateDef, typename _Enable = void>
struct initial_recursive;

template <typename _StateDef>
struct initial_recursive<_StateDef, std::enable_if_t<std::is_same_v<SimpleStateType, meta_type_t<_StateDef>>>>
{
    using type = _StateDef;
};

template <typename _StateDef>
using initial_recursive_t = typename initial_recursive<_StateDef>::type;

template <typename _StateDef>
struct initial_recursive<_StateDef, std::enable_if_t<is_initial_specified_v<_StateDef>>>
{
    using type = initial_recursive<typename _StateDef::Initial>;
};

template <typename _StateDef>
struct initial_recursive<_StateDef, std::enable_if_t<
        !is_initial_specified_v<_StateDef>
     && !std::is_same_v<SimpleStateType, meta_type_t<_StateDef>>
    >>
{
    using type = initial_recursive_t<std::tuple_element_t<0, typename _StateDef::SubStates>>;
};



template <typename _StateDefTuple, typename _SubStateDef>
class containing_state;

template <typename ... _StateDef, typename _SubStateDef>
struct containing_state<std::tuple<_StateDef...>, _SubStateDef>
{
    using type = typename collapse<
        std::conditional_t<
            is_in_context_recursive_v<_SubStateDef, _StateDef>,
            _StateDef,
            void
        >...>::type;
};

template <typename _StateDefTuple, typename _SubStateDef>
using containing_state_t = typename containing_state<_StateDefTuple, _SubStateDef>::type;



template <typename _StateDef, typename _MetaType>
struct mixin;

template <typename _StateDef>
struct SimpleStateMixin;

template <typename _StateDef>
struct CompositeStateMixin;

template <typename _StateDef>
struct TopStateMixin;

template <typename _StateDef>
struct mixin<_StateDef, SimpleStateType>
{
    using type = SimpleStateMixin<_StateDef>;
};

template <typename _StateDef>
struct mixin<_StateDef, CompositeStateType>
{
    using type = CompositeStateMixin<_StateDef>;
};

template <typename _StateDef>
struct mixin<_StateDef, TopStateType>
{
    using type = TopStateMixin<_StateDef>;
};


template <typename _StateDef>
using mixin_t = typename mixin<_StateDef, meta_type_t<_StateDef>>::type;

}