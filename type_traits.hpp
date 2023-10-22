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

template <typename _StateDef, typename _Enable = void>
struct contained_states;

template <typename _StateDef>
struct contained_states<_StateDef, std::enable_if_t<is_simple_state_v<_StateDef>>>
{
    using direct = std::tuple<>;
    using recursive = std::tuple<>;
};

template <typename _StateDef>
struct contained_states<_StateDef, std::enable_if_t<is_composite_state_v<_StateDef>>>
{
    using direct = typename _StateDef::SubStates;
    using recursive = tuple_join_t<direct, typename contained_states<direct>::recursive>;
};

template <typename _StateDef>
struct contained_states<_StateDef, std::enable_if_t<is_orthogonal_state_v<_StateDef>>>
{
    using direct = typename _StateDef::Regions;
    using recursive = tuple_join_t<direct, typename contained_states<direct>::recursive>;
};

template <typename ... _StateDef>
struct contained_states<std::tuple<_StateDef...>, void>
{
    using recursive = tuple_join_t<typename contained_states<_StateDef>::recursive...>;
};

template <typename _StateDef>
using contained_states_direct_t = typename contained_states<_StateDef>::direct;

template <typename _StateDef>
using contained_states_recursive_t = typename contained_states<_StateDef>::recursive;

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
std::size_t direct_substate_to_enter_f(std::size_t target_combination, state_spec<std::tuple<_SubStateDef...>>) {
    std::size_t substate_local_id = 0;
    (static_cast<bool>(substate_local_id++, state_combination_recursive_v<_SubStateDef> & target_combination) || ...)
        || (substate_local_id++, true);
    return substate_local_id - 1;
}

template <typename _StateDef, typename _Enable = void>
class mixin;

template <typename _StateDef>
class SimpleStateMixin;

template <typename _StateDef>
class CompositeStateMixin;

template <typename _StateDef>
class OrthogonalStateMixin;

template <typename _StateMixin>
class TopStateMixin;

class RootStateMixin;

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
struct mixin<_StateDef, std::enable_if_t<is_simple_state_v<_StateDef> && is_top_state_v<_StateDef>>>
{
    using type = TopStateMixin<SimpleStateMixin<_StateDef>>;
};

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_composite_state_v<_StateDef> && is_top_state_v<_StateDef>>>
{
    using type = TopStateMixin<CompositeStateMixin<_StateDef>>;
};

template <typename _StateDef>
struct mixin<_StateDef, std::enable_if_t<is_orthogonal_state_v<_StateDef> && is_top_state_v<_StateDef>>>
{
    using type = TopStateMixin<OrthogonalStateMixin<_StateDef>>;
};

template <typename ... _T>
struct mixin<std::tuple<_T...>>
{
    using type = std::tuple<typename mixin<_T>::type...>;
};

template <>
struct mixin<void, void>
{
    using type = RootStateMixin;
};

template <typename _StateDef>
using mixin_t = typename mixin<_StateDef>::type;

auto operator+(std::tuple<bool, std::size_t> lhs, std::tuple<bool, std::size_t> rhs) {
    return std::make_tuple(std::get<0>(lhs) || std::get<0>(rhs), std::get<1>(lhs) | std::get<1>(rhs));
}

template <typename ... _StateDef>
void remove_conflicting(std::size_t& target_combination, state_spec<std::tuple<_StateDef...>>) {
    bool already_matched = false;
    ((already_matched ? target_combination &= ~state_combination_recursive_v<_StateDef> : (already_matched = target_combination & state_combination_recursive_v<_StateDef>)), ...);
}

}