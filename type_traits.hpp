#pragma once
#include <type_traits>
#include <tuple>
#include <variant>
#include <iostream>

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
struct has_reaction_to_event<_StateDef, _Event, std::void_t<std::is_invocable<decltype(&_StateDef::react), _StateDef&, const _Event&>>>
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
constexpr std::size_t state_id_v = index_v<_StateDef, all_states_t<typename _StateDef::TopStateDef>>;

template <typename _StateDef>
struct state_combination
{
    static constexpr std::size_t value = 1 << state_id_v<_StateDef>;
};

template <typename ... _StateDef>
struct state_combination<std::tuple<_StateDef...>>
{
    static constexpr std::size_t value = ((1 << state_id_v<_StateDef>) | ...);
};

template <>
struct state_combination<std::tuple<>>
{
    static constexpr std::size_t value = 0;
};

template <typename _StateDef>
constexpr std::size_t state_combination_recursive_v = state_combination<all_states_t<_StateDef>>::value;

template <typename _StateDef>
constexpr std::size_t state_combination_v = state_combination<_StateDef>::value;

template <typename _StateDef, typename _ContextDef>
constexpr bool is_in_context_recursive_v = state_combination_v<_StateDef> & state_combination_recursive_v<_ContextDef>;

template <typename _StateDef, typename _AllStateDefs>
struct super_state;

template <typename _StateDef, typename ... _OtherStateDef>
struct super_state<_StateDef, std::tuple<_OtherStateDef...>>
{
    using type = first_non_void_t<
        RootState,
        std::conditional_t<
            static_cast<bool>(state_combination_v<contained_states_direct_t<_OtherStateDef>> & state_combination_v<_StateDef>),
            _OtherStateDef,
            void>...
        >;
};

template <typename _StateDef>
using super_state_t  = typename super_state<_StateDef, all_states_t<typename _StateDef::TopStateDef>>::type;

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

template <typename _StateDef, typename ... _SubStateDef>
std::size_t compute_direct_substate(std::size_t current_local, std::size_t initial, std::size_t shallow, std::size_t deep, type_identity<std::tuple<_SubStateDef...>>) {
    std::size_t substate_local_id = 0;
    ((std::cout<< state_combination_v<_SubStateDef> << std::endl), ...);
    (static_cast<bool>(substate_local_id++, state_combination_recursive_v<_SubStateDef> & (initial | shallow | deep)) || ...)
        || (substate_local_id++, true);
    substate_local_id--;
        
    if (substate_local_id == sizeof...(_SubStateDef) && state_combination_v<_StateDef> & (shallow | deep)) {
        return current_local;
    }
    else {
        return substate_local_id;
    }
}

template <typename _StateDef>
class SimpleStateWrapper;

template <typename _StateDef>
class CompositeStateWrapper;

template <typename _StateDef>
class OrthogonalStateWrapper;

template <typename _StateMixin, typename _StateBase = base_t<typename _StateMixin::StateDef>>
struct wrapper;

template <typename _StateMixin>
struct wrapper<_StateMixin, SimpleStateBase> { using type = SimpleStateWrapper<_StateMixin>; };

template <typename _StateMixin>
struct wrapper<_StateMixin, CompositeStateBase> { using type = CompositeStateWrapper<_StateMixin>; };

template <typename _StateMixin>
struct wrapper<_StateMixin, OrthogonalStateBase> { using type = OrthogonalStateWrapper<_StateMixin>; };

template <typename _StateMixin>
using wrapper_t = typename wrapper<_StateMixin>::type;

template <typename _StateMixins>
struct wrappers;

template <typename ... _StateMixin>
struct wrappers<std::tuple<_StateMixin...>>
{
    using type = std::tuple<wrapper_t<_StateMixin>...>;
};

template <typename _StateMixins>
using wrappers_t = typename wrappers<_StateMixins>::type;

template <typename _StateDef>
class StateMixin;

template <typename _StateDefs>
struct mixins;

template <typename ... _StateDef>
struct mixins<std::tuple<_StateDef...>>
{
    using type = std::tuple<StateMixin<_StateDef>...>;
};

template <typename _StateDefs>
using mixins_t = typename mixins<_StateDefs>::type;

template <typename ... _StateDef>
void remove_conflicting(std::size_t& initial, std::size_t& shallow, std::size_t& deep, type_identity<std::tuple<_StateDef...>>) {
    bool already_matched = false;
    auto do_remove = [&](std::size_t target){
        ((
            already_matched ? (
                initial &= ~state_combination_recursive_v<_StateDef>,
                shallow &= ~state_combination_recursive_v<_StateDef>,
                deep &= ~state_combination_recursive_v<_StateDef>
            )
            : (
                already_matched = target & state_combination_recursive_v<_StateDef>
            )
        ), ...);
    };
    do_remove(initial | shallow | deep);
}

}