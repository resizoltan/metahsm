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

template <typename _StateDef>
constexpr std::size_t state_count_v = std::tuple_size_v<all_states_t<_StateDef>>;

template <typename _StateDef>
constexpr std::size_t state_combination_count_v = (1 << state_count_v<_StateDef> + 1);

template <typename _StateDef, typename _ContextDef>
constexpr bool is_in_context_recursive_v = (1 << state_id_v<_StateDef>) & state_combination_recursive_v<_ContextDef>;

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

template <typename ... _SubStateDef>
std::size_t direct_substate_to_enter_f(std::size_t target_combination, type_identity<std::tuple<_SubStateDef...>>) {
    std::size_t substate_local_id = 0;
    (static_cast<bool>(substate_local_id++, state_combination_recursive_v<_SubStateDef> & target_combination) || ...)
        || (substate_local_id++, true);
    return substate_local_id - 1;
}

template <typename _StateDef>
class SimpleStateMixin;

template <typename _StateDef>
class CompositeStateMixin;

template <typename _StateDef>
class OrthogonalStateMixin;

template <typename _StateMixin>
class TopStateMixin;

class RootStateMixin;

template <typename _StateDef, typename _StateBase = base_t<_StateDef>>
struct mixin;

template <typename _StateDef>
struct mixin<_StateDef, SimpleStateBase> { using type = SimpleStateMixin<_StateDef>; };

template <typename _StateDef>
struct mixin<_StateDef, CompositeStateBase> { using type = CompositeStateMixin<_StateDef>; };

template <typename _StateDef>
struct mixin<_StateDef, OrthogonalStateBase> { using type = OrthogonalStateMixin<_StateDef>; };

template <typename _StateDef>
struct mixin<_StateDef, RootState> { using type = RootStateMixin; };

template <typename _StateDef>
using mixin_t = std::conditional_t<is_top_state_v<_StateDef>, TopStateMixin<typename mixin<_StateDef>::type>, typename mixin<_StateDef>::type>;

template <typename _StateDefs>
struct mixins;

template <typename ... _StateDef>
struct mixins<std::tuple<_StateDef...>>
{
    using type = std::tuple<mixin_t<_StateDef>...>;
};

template <typename _StateDefs>
using mixins_t = typename mixins<_StateDefs>::type;


auto operator+(std::tuple<bool, std::size_t> lhs, std::tuple<bool, std::size_t> rhs) {
    return std::make_tuple(std::get<0>(lhs) || std::get<0>(rhs), std::get<1>(lhs) | std::get<1>(rhs));
}

template <typename ... _StateDef>
void remove_conflicting(std::size_t& target_combination, type_identity<std::tuple<_StateDef...>>) {
    bool already_matched = false;
    ((already_matched ? target_combination &= ~state_combination_recursive_v<_StateDef> : (already_matched = target_combination & state_combination_recursive_v<_StateDef>)), ...);
}

}