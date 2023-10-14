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
struct is_orthogonal_state<_Entity, std::void_t<std::tuple<typename _Entity::SubStates>>> : std::false_type {};

template <typename _Entity>
struct is_orthogonal_state<_Entity, std::void_t<std::tuple<typename _Entity::Regions>>> : std::true_type {};

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

template <typename ... _StateDef>
struct state_spec<std::tuple<_StateDef...>> : public std::tuple<state_spec<_StateDef>...>
{
    using type = std::tuple<_StateDef...>;
};

template <typename _StateDef>
using state_spec_t = typename state_spec<_StateDef>::type;

template <typename _StateDef, typename _Event, typename _SFINAE = void>
struct has_reaction_to_event : public std::false_type
{};

template <typename _StateDef, typename _Event>
struct has_reaction_to_event<_StateDef, _Event, std::void_t<std::is_invocable<decltype(&_StateDef::react), _StateDef&, const _Event&>>>
: std::true_type
{};

template <typename _StateDef, typename _Event>
constexpr bool has_reaction_to_event_v = has_reaction_to_event<_StateDef, _Event>::value;

template <typename _StateDef, typename _SFINAE = void>
struct has_entry_action : public std::false_type
{};

template <typename _StateDef>
struct has_entry_action<_StateDef, std::void_t<std::is_invocable<decltype(&_StateDef::on_entry), _StateDef&>>>
: std::true_type
{};

template <typename _StateDef>
constexpr bool has_entry_action_v = has_entry_action<_StateDef>::value;

template <typename _StateDef, typename _SFINAE = void>
struct has_exit_action : public std::false_type
{};

template <typename _StateDef>
struct has_exit_action<_StateDef, std::void_t<std::is_invocable<decltype(&_StateDef::on_exit), _StateDef&>>>
: std::true_type
{};

template <typename _StateDef>
constexpr bool has_exit_action_v = has_exit_action<_StateDef>::value;

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
struct is_in_context_recursive<_StateDef, _ContextDef, std::enable_if_t<
        !std::is_same_v<_StateDef, _ContextDef> &&
        !std::is_void_v<typename _ContextDef::Regions>
    >>
: is_in_context_recursive<_StateDef, typename _ContextDef::Regions>
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

template <typename _StateDef, typename _SubStateOfPotentialSuperstate, typename _PotentialSuperState>
struct super_state<_StateDef, _SubStateOfPotentialSuperstate, _PotentialSuperState, std::enable_if_t<
        !std::is_void_v<typename _SubStateOfPotentialSuperstate::Regions> &&
        !std::is_same_v<_StateDef, _SubStateOfPotentialSuperstate> &&
        is_in_context_recursive_v<_StateDef, _SubStateOfPotentialSuperstate>
    >>
{
    using SubType = typename super_state<_StateDef, typename _SubStateOfPotentialSuperstate::Regions, _SubStateOfPotentialSuperstate>::type;
    using type = typename first_non_void<SubType, _SubStateOfPotentialSuperstate>::type;
};

template <typename _StateDef, typename _PotentialSuperState>
struct super_state<_StateDef, _StateDef, _PotentialSuperState, void>
{
    using type = _PotentialSuperState;
};

template <typename _StateDef>
using super_state_t  = typename super_state<_StateDef, typename _StateDef::TopStateDef, typename _StateDef::TopStateDef>::type;

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
struct initial_state<_StateDef, std::void_t<typename _StateDef::Regions>>
{
    using type = typename initial_state<typename _StateDef::Regions>::type;
};

template <typename ... _RegionDef>
struct initial_state<std::tuple<_RegionDef...>, void>
{
    using type = std::tuple<typename initial_state<_RegionDef>::type...>;
};

template <typename _StateDef>
using initial_state_t = typename initial_state<_StateDef>::type;

template <typename _StateDef, typename _SubStateDefTuple, typename ... _TargetStateDef>
struct direct_substate_to_enter;

template <typename _StateDef, typename ... _SubStateDef, typename ... _TargetStateDef>
struct direct_substate_to_enter<_StateDef, std::tuple<_SubStateDef...>, _TargetStateDef...>
{
    using type = typename first_non_void<
        std::conditional_t<
            is_any_in_context_recursive_v<std::tuple<_TargetStateDef...>, _SubStateDef>,
            _SubStateDef,
            void
        >...,
        initial_state_t<_StateDef>>::type;
};

template <typename _StateDef, typename ... _TargetStateDef>
using direct_substate_to_enter_t = typename direct_substate_to_enter<_StateDef, typename _StateDef::SubStates, _TargetStateDef...>::type;

template <typename _StateDef, typename _Enable = void>
struct all_states;

template <typename _StateDef>
struct all_states<_StateDef, std::enable_if_t<is_simple_state_v<_StateDef>>>
{
    using type = std::tuple<_StateDef>;
};

template <typename ... _StateDef>
struct all_states<std::tuple<_StateDef...>, void>
{
    using type = tuple_join_t<typename all_states<_StateDef>::type...>;
};

template <typename _StateDef>
struct all_states<_StateDef, std::enable_if_t<is_composite_state_v<_StateDef>>>
{
    using type = tuple_add_t<_StateDef, typename all_states<typename _StateDef::SubStates>::type>;
};

template <typename _StateDef>
struct all_states<_StateDef, std::enable_if_t<is_orthogonal_state_v<_StateDef>>>
{
    using type = tuple_add_t<_StateDef, typename all_states<typename _StateDef::Regions>::type>;
};

template <typename _StateDef>
using all_states_t = typename all_states<_StateDef>::type;

template <typename _StateDef>
constexpr std::size_t state_id_v = index_v<_StateDef, all_states_t<typename _StateDef::TopStateDef>>;

template <typename _StateDef>
constexpr std::size_t state_count_v = std::tuple_size_v<all_states_t<_StateDef>>;

template <typename _StateDef>
constexpr std::size_t state_combination_count_v = (1 << state_count_v<_StateDef> + 1);

template <typename _StateDef, typename _Enable = void>
struct mixin;

template <typename _StateDef>
struct SimpleStateMixin;

template <typename _StateDef>
struct CompositeStateMixin;

template <typename _StateDef>
struct OrthogonalStateMixin;

template <typename _StateDef>
struct CompositeTopStateMixin;

template <typename _StateDef>
struct OrthogonalTopStateMixin;

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
struct mixin<_StateDef, std::enable_if_t<is_orthogonal_state_v<_StateDef> && !is_top_state_v<_StateDef>>>
{
    using type = OrthogonalStateMixin<_StateDef>;
};

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_composite_state_v<_StateDef> && is_top_state_v<_StateDef>>>
{
    using type = CompositeTopStateMixin<_StateDef>;
};

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_orthogonal_state_v<_StateDef> && is_top_state_v<_StateDef>>>
{
    using type = OrthogonalTopStateMixin<_StateDef>;
};

template <typename ... _T>
struct mixin<std::tuple<_T...>>
{
    using type = std::tuple<typename mixin<_T>::type...>;
};

template <typename _StateDef>
using mixin_t = typename mixin<_StateDef>::type;

}