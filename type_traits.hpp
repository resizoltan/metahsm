#pragma once
#include <type_traits>
#include <tuple>
#include <variant>

#include "type_algorithms.hpp"

namespace metahsm {

struct EntityBase {};
struct StateBase : EntityBase {};
struct SimpleStateBase : StateBase {};
struct CompositeStateBase : StateBase {};
struct TopStateBase : CompositeStateBase {};

template <typename _Entity, typename _Enable = void>
struct is_state : std::false_type {};

template <typename _Entity>
struct is_state<_Entity, std::enable_if_t<std::is_base_of_v<StateBase, _Entity>>> : std::true_type {};

template <typename _Entity, typename _SFINAE = void>
struct is_simple_state : is_state<_Entity> {};

template <typename _Entity>
struct is_simple_state<_Entity, std::void_t<std::tuple<typename _Entity::SubStates>>> : std::false_type {};

template <typename _Entity>
struct is_simple_state<_Entity, std::void_t<std::tuple<typename _Entity::Regions>>> : std::false_type {};

template <typename _Entity>
constexpr bool is_simple_state_v = is_simple_state<_Entity>::value;

template <typename _Entity, typename _SFINAE = void>
struct is_composite_state : std::false_type {};

template <typename _Entity>
struct is_composite_state<_Entity, std::void_t<std::tuple<typename _Entity::SubStates>>> : std::true_type {};

template <typename _Entity>
struct is_composite_state<_Entity, std::void_t<std::tuple<typename _Entity::Regions>>> : std::false_type {};

template <typename _Entity>
constexpr bool is_composite_state_v = is_composite_state<_Entity>::value;

template <typename _Entity, typename _SFINAE = void>
struct is_orthogonal_state : std::false_type {};

template <typename _Entity>
struct is_orthogonal_state<_Entity, std::void_t<std::tuple<typename _Entity::SubStates>>> : std::true_type {};

template <typename _Entity>
struct is_orthogonal_state<_Entity, std::void_t<std::tuple<typename _Entity::Regions>>> : std::false_type {};

template <typename _Entity>
constexpr bool is_orthogonal_state_v = is_orthogonal_state<_Entity>::value;

template <typename _Entity, typename _Enable = void>
struct is_top_state : std::false_type {};

template <typename _Entity>
struct is_top_state<_Entity, std::enable_if_t<std::is_base_of_v<TopStateBase, _Entity>>> : std::true_type {};

template <typename _Entity>
constexpr bool is_top_state_v = is_top_state<_Entity>::value;

template <typename _StateDef>
struct state_spec
{
    using type = _StateDef;
};

template <typename _StateDef>
using state_spec_t = typename state_spec<_StateDef>::type;

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
struct is_in_context_recursive<_StateDef, std::tuple<_ContextDef...>>
: std::disjunction<is_in_context_recursive<_StateDef, _ContextDef>...>
{};

template <typename ... _StateDef, typename _ContextDef>
struct is_in_context_recursive<std::tuple<_StateDef...>, _ContextDef> 
: std::disjunction<is_in_context_recursive<_StateDef, _ContextDef>...>
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

template <typename _StateDef, typename _ContextDef>
struct is_any_in_context_recursive : is_in_context_recursive<_StateDef, _ContextDef> 
{};

template <typename ... _StateDef, typename _ContextDef>
struct is_any_in_context_recursive<std::tuple<_StateDef...>, _ContextDef>
: std::disjunction<is_in_context_recursive<_StateDef, _ContextDef>...>
{};

template <typename _StateDef, typename _ContextDef>
constexpr bool is_any_in_context_recursive_v = is_any_in_context_recursive<_StateDef, _ContextDef>::value;

template <typename _StateDef, typename _StateDefToCompare, typename _PotentialSuperState, typename Enable = void>
struct super_state
{
    using type = void;
};

template <typename _StateDef, typename ... _SubStateOfPotentialSuperstate, typename _PotentialSuperState>
struct super_state<_StateDef, std::tuple<_SubStateOfPotentialSuperstate...>, _PotentialSuperState, void>
{
    using type = typename first_non_void<typename super_state<_StateDef, _SubStateOfPotentialSuperstate, _PotentialSuperState>::type...>::type;
};

template <typename _StateDef, typename _SubStateOfPotentialSuperstate, typename _PotentialSuperState>
struct super_state<_StateDef, _SubStateOfPotentialSuperstate, _PotentialSuperState, std::enable_if_t<
        !std::is_void_v<typename _SubStateOfPotentialSuperstate::SubStates> &&
        !std::is_same_v<_StateDef, _SubStateOfPotentialSuperstate> &&
        is_in_context_recursive_v<_StateDef, _SubStateOfPotentialSuperstate>
    >>
{
    using SubType = typename super_state<_StateDef, typename _SubStateOfPotentialSuperstate::SubStates, _SubStateOfPotentialSuperstate>::type;
    using type = typename first_non_void<SubType, _SubStateOfPotentialSuperstate>::type;
};

template <typename _StateDef, typename _PotentialSuperState>
struct super_state<_StateDef, _StateDef, _PotentialSuperState, void>
{
    using type = _PotentialSuperState;
};

template <typename _StateDef>
using super_state_t  = typename super_state<_StateDef, typename _StateDef::TopStateDef, typename _StateDef::TopStateDef>::type;

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
    using type = typename first_non_void<typename lca<_StateDef1, _StateDef2, _ContextDef>::type...>::type;
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
    using type = typename first_non_void<SubType, _ContextDef>::type;
};

template <typename _StateDef1, typename _StateDef2>
using lca_t = typename lca<_StateDef1, _StateDef2, typename _StateDef1::TopStateDef>::type;

template <typename _StateDef, typename SFINAE = void>
struct initial_state 
{
    using type = std::tuple_element_t<0, typename _StateDef::SubStates>;
};

template <typename _StateDef>
struct initial_state<_StateDef, std::void_t<typename _StateDef::Initial>>
{
    using type = typename _StateDef::Initial;
};

template <typename _StateDef>
using initial_state_t = typename initial_state<_StateDef>::type;

template <typename _StateDef, typename _SubStateDefTuple, typename _TargetStateDef>
struct direct_substate_to_enter;

template <typename _StateDef, typename ... _SubStateDef, typename _TargetStateDef>
struct direct_substate_to_enter<_StateDef, std::tuple<_SubStateDef...>, _TargetStateDef>
{
    using type = typename first_non_void<
        std::conditional_t<
            is_any_in_context_recursive_v<_TargetStateDef, _SubStateDef>,
            _SubStateDef,
            void
        >...,
        initial_state_t<_StateDef>>::type;
};

template <typename _StateDef, typename _TargetStateDef>
using direct_substate_to_enter_t = typename direct_substate_to_enter<_StateDef, typename _StateDef::SubStates, _TargetStateDef>::type;

template <typename _StateDef, typename _Enable = void>
struct mixin;

template <typename _StateDef>
struct SimpleStateMixin;

template <typename _StateDef>
struct CompositeStateMixin;

template <typename _StateDef>
struct TopStateMixin;

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_simple_state_v<_StateDef>>>
{
    using type = SimpleStateMixin<_StateDef>;
};

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_composite_state_v<_StateDef> && !is_top_state_v<_StateDef>>>
{
    using type = CompositeStateMixin<_StateDef>;
};

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_top_state_v<_StateDef>>>
{
    using type = TopStateMixin<_StateDef>;
};

template <typename ... _T>
struct mixin<std::tuple<_T...>>
{
    using type = std::tuple<typename mixin<_T>::type...>;
};

template <typename _StateDef>
using mixin_t = typename mixin<_StateDef>::type;

}