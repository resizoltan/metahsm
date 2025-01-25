#pragma once
#include <type_traits>
#include <tuple>
#include <variant>
#include <iostream>
#include <bitset>

#include "type_algorithms.hpp"

namespace metahsm {

struct EntityBase {};
struct StateBase : EntityBase {};
struct SimpleStateBase : StateBase {};
struct CompositeStateBase : StateBase {};
struct OrthogonalStateBase : StateBase {};
struct TopStateBase {};
struct RootState : StateBase {};

template <typename _Entity, typename _SFINAE = void>
struct has_regions : std::false_type {};

template <typename _Entity>
struct has_regions<_Entity, std::void_t<std::tuple<typename _Entity::Regions>>> : std::true_type {};

template <typename _Entity>
constexpr bool has_regions_v = has_regions<_Entity>::value;

template <typename _Entity, typename _SFINAE = void>
struct has_substates : std::false_type {};

template <typename _Entity>
struct has_substates<_Entity, std::void_t<std::tuple<typename _Entity::SubStates>>> : std::true_type {};

template <typename _Entity>
constexpr bool has_substates_v = has_substates<_Entity>::value;

template <typename _Entity, typename _SFINAE = void>
struct has_initial : std::false_type {};

template <typename _Entity>
struct has_initial<_Entity, std::void_t<std::tuple<typename _Entity::Initial>>> : std::true_type {};

template <typename _Entity>
constexpr bool has_initial_v = has_initial<_Entity>::value;

template <bool has_substates, bool has_regions>
struct base;

template <>
struct base<false, false> { using type = SimpleStateBase; };

template <>
struct base<true, false> { using type = CompositeStateBase; };

template <>
struct base<false, true> { using type = OrthogonalStateBase; };

template <typename _Entity>
constexpr bool is_state_v = std::is_base_of_v<StateBase, _Entity>;

template <typename _Entity>
constexpr bool is_top_state_v = std::is_base_of_v<TopStateBase, _Entity>;

template <typename _Entity>
constexpr bool is_root_state_v = std::is_same_v<_Entity, RootState>;

template <typename _Entity>
constexpr bool is_simple_state_v = std::is_base_of_v<SimpleStateBase, _Entity>;

template <typename _Entity>
using base_t = std::conditional_t<
    is_state_v<_Entity>, 
    std::conditional_t<
        is_root_state_v<_Entity>,
        RootState,
        typename base<has_substates_v<_Entity>, has_regions_v<_Entity>>::type>, 
    void>;

template <typename _StateDef, typename _Event, typename _SFINAE = void>
struct has_reaction_to_event : std::false_type
{};

template <typename _StateDef, typename _Event>
struct has_reaction_to_event<_StateDef, _Event, std::void_t<std::enable_if_t<std::is_invocable_v<decltype(&_StateDef::react), _StateDef&, const _Event&>>>>
: std::true_type
{};

template <typename _StateDef, typename _Event>
constexpr bool has_reaction_to_event_v = has_reaction_to_event<_StateDef, _Event>::value;

template <typename _StateDef, typename _SFINAE = void>
struct has_entry_action : std::false_type
{};

template <typename _StateDef>
struct has_entry_action<_StateDef, std::void_t<std::is_invocable<decltype(&_StateDef::on_entry), _StateDef&>>>
: std::true_type
{};

template <typename _StateDef>
constexpr bool has_entry_action_v = has_entry_action<_StateDef>::value;

template <typename _StateDef, typename _SFINAE = void>
struct has_exit_action : std::false_type
{};

template <typename _StateDef>
struct has_exit_action<_StateDef, std::void_t<std::is_invocable<decltype(&_StateDef::on_exit), _StateDef&>>>
: std::true_type
{};

template <typename _StateDef>
constexpr bool has_exit_action_v = has_exit_action<_StateDef>::value;

template <typename _StateDef, typename _StateBase = void>
struct contained_states;

template <typename _StateDef>
struct contained_states<_StateDef, SimpleStateBase>
{
    using direct = std::tuple<>;
    using all = std::tuple<>;
};

template <typename _StateDef>
struct contained_states<_StateDef, CompositeStateBase>
{
    using direct = typename _StateDef::SubStates;
    using all = tuple_join_t<direct, typename contained_states<direct>::all>;
};

template <typename _StateDef>
struct contained_states<_StateDef, OrthogonalStateBase>
{
    using direct = typename _StateDef::Regions;
    using all = tuple_join_t<direct, typename contained_states<direct>::all>;
};

template <typename ... _StateDef>
struct contained_states<std::tuple<_StateDef...>, void>
{
    using all = tuple_join_t<typename contained_states<_StateDef, base_t<_StateDef>>::all...>;
};

template <typename _StateDef>
using contained_states_direct_t = typename contained_states<_StateDef, base_t<_StateDef>>::direct;

template <typename _StateDef>
using contained_states_recursive_t = typename contained_states<_StateDef, base_t<_StateDef>>::all;

template <typename _StateDef>
using all_states_t = tuple_join_t<_StateDef, contained_states_recursive_t<_StateDef>>;


template <typename _StateDef>
struct state_id
{
    static constexpr std::size_t value = index_v<_StateDef, all_states_t<typename _StateDef::TopState>>;
};

template <>
struct state_id<RootState>
{
    static constexpr std::size_t value = 0;
};

template <typename _StateDef>
constexpr std::size_t state_id_v = state_id<_StateDef>::value;

template <typename _State>
using state_combination_t = std::bitset<std::tuple_size_v<all_states_t<typename _State::TopState>>>;

template <typename _StateDef>
auto state_combination(type_identity<_StateDef>)
{
    state_combination_t<typename _StateDef::TopState> value;
    value.set(state_id_v<_StateDef>);
    return value;
};

template <typename _State1, typename ... _StateDef>
auto state_combination(type_identity<std::tuple<_State1, _StateDef...>>)
{
    state_combination_t<typename _State1::TopState> value;
    value.set(state_id_v<_State1>);
    (value.set(state_id_v<_StateDef>), ...);
    return value;
};


template <typename _StateDef>
const auto state_combination_recursive_v = state_combination(type_identity<all_states_t<_StateDef>>{});

template <typename _StateDef>
const auto state_combination_v = state_combination(type_identity<_StateDef>{});

template <typename _StateDef, typename _ContextDef>
constexpr bool is_in_context_recursive_v = state_combination_v<_StateDef> & state_combination_recursive_v<_ContextDef>;

template <typename _StateDef, typename _AllStateDefs>
struct super_state;

template <typename _StateDef, typename ... _OtherStateDef>
struct super_state<_StateDef, std::tuple<_OtherStateDef...>>
{
    using direct = first_non_void_t<
        RootState,
        std::conditional_t<
            static_cast<bool>(state_combination_v<contained_states_direct_t<_OtherStateDef>> & state_combination_v<_StateDef>),
            _OtherStateDef,
            void>...
        >;
    using recursive = tuple_join_t<direct, typename super_state<direct, std::tuple<_OtherStateDef...>>::recursive>;
};

template <typename ... _OtherStateDef>
struct super_state<RootState, std::tuple<_OtherStateDef...>>
{
    using recursive = std::tuple<>; 
};

template <typename _StateDef>
using super_state_direct_t  = typename super_state<_StateDef, all_states_t<typename _StateDef::TopStateDef>>::direct;

template <typename _StateDef>
using super_state_recursive_t  = typename super_state<_StateDef, all_states_t<typename _StateDef::TopStateDef>>::recursive;

template <typename _StateDef, typename _StateBase = base_t<_StateDef>>
struct default_initial_state;

template <typename _StateDef, bool has_initial = has_initial_v<_StateDef>>
struct initial_state
{
    using type = typename _StateDef::Initial;
};

template <typename _StateDef>
struct initial_state<_StateDef, false>
{
    using type = typename default_initial_state<_StateDef>::type;
};

template <typename _RegionDefs>
struct initial_states;

template <typename ... _RegionDef>
struct initial_states<std::tuple<_RegionDef...>>
{
    using type = std::tuple<typename initial_state<_RegionDef>::type...>;
};

template <typename _StateDef>
struct default_initial_state<_StateDef, CompositeStateBase>
{
    using type = std::tuple_element_t<0, typename _StateDef::SubStates>;
};

template <typename _StateDef>
struct default_initial_state<_StateDef, OrthogonalStateBase>
{
    using type = typename initial_states<typename _StateDef::Regions>::type;
};

template <typename _StateDef>
using initial_state_t = typename initial_state<_StateDef>::type;

template <typename _StateDef, typename _StateBase = base_t<_StateDef>>
struct initial_state_combination;

template <typename _StateDef>
using initial_state_combination_t = typename initial_state_combination<_StateDef>::type;

template <typename _StateDef>
constexpr std::size_t initial_state_combination_v = state_combination_v<initial_state_combination_t<_StateDef>>;

template <typename _StateDef>
struct initial_state_combination<_StateDef, SimpleStateBase>
{
    using type = std::tuple<>;
};

template <typename _StateDef>
struct initial_state_combination<_StateDef, CompositeStateBase>
{
    using type = tuple_join_t<initial_state_t<_StateDef>, initial_state_combination_t<initial_state_t<_StateDef>>>; 
};

template <typename _StateDef>
struct initial_state_combination<_StateDef, OrthogonalStateBase>
{
    using type = tuple_apply_t<initial_state_combination_t, contained_states_direct_t<_StateDef>>; 
};

template <typename _StateDef, typename ... _SubStateDef>
std::size_t compute_direct_substate(std::size_t target, type_identity<std::tuple<_SubStateDef...>>) {
    std::size_t substate_local_id = 0;
    bool found = (static_cast<bool>(substate_local_id++, state_combination_recursive_v<_SubStateDef> & target) || ...)
        || (substate_local_id++, true);
    return found ? substate_local_id - 1 : 0;
}

template <typename _StateDef>
class SimpleStateWrapper;

template <typename _StateDef>
class CompositeStateWrapper;

template <typename _StateDef>
class OrthogonalStateWrapper;

template <typename State_, typename _StateBase = base_t<State_>>
struct wrapper;

template <typename State_>
struct wrapper<State_, SimpleStateBase> { using type = SimpleStateWrapper<State_>; };

template <typename State_>
struct wrapper<State_, CompositeStateBase> { using type = CompositeStateWrapper<State_>; };

template <typename State_>
struct wrapper<State_, OrthogonalStateBase> { using type = OrthogonalStateWrapper<State_>; };

template <typename State_>
using wrapper_t = typename wrapper<State_>::type;

template <typename _TopState>
bool is_legal_state_combination(std::size_t state_combination) {
    auto is_legal_for_state = [&](auto state_identity) {
        using StateDef = typename decltype(state_identity)::type;
        if constexpr(has_substates_v<StateDef>) {
            int number_of_contained_branches = 0;
            auto contains_branch = [&](auto ... branch_identity) {
                number_of_contained_branches = (((state_combination_v<typename decltype(branch_identity)::type> & state_combination) ? 1 : 0) + ...);
            };
            std::apply(contains_branch, type_identity_tuple<contained_states_direct_t<StateDef>>{});
            if(number_of_contained_branches > 1) {
                return false;
            }
        }
        return true;
    };
    auto is_legal_for_states = [&] (auto ... state_identity) {
        return (is_legal_for_state(state_identity) && ...);
    };
    return std::apply(is_legal_for_states, type_identity_tuple<all_states_t<_TopState>>{});
}

}