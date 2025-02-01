#pragma once
#include <type_traits>
#include <tuple>
#include <variant>
#include <iostream>
#include <bitset>
#include <cstdint>

#include "type_algorithms.hpp"

namespace metahsm {

struct EntityBase {};
struct StateBase : EntityBase {};
struct SimpleStateBase : StateBase {};
struct CompositeStateBase : StateBase {};
struct OrthogonalStateBase : StateBase {};
struct TopStateBase {};
struct RootState : StateBase {};
class StateImplBase;

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
constexpr bool is_state_v = std::is_base_of_v<StateImplBase, _Entity>;

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

template <typename State_, typename StateBase_ = void>
struct contained_states;

template <typename State_>
struct contained_states<State_, SimpleStateBase>
{
    using direct = std::tuple<>;
    using all = std::tuple<>;
};

template <typename State_>
struct contained_states<State_, CompositeStateBase>
{
    using direct = typename State_::SubStates;
    using all = tuple_join_t<direct, typename contained_states<direct>::all>;
};

template <typename State_>
struct contained_states<State_, OrthogonalStateBase>
{
    using direct = typename State_::Regions;
    using all = tuple_join_t<direct, typename contained_states<direct>::all>;
};

template <typename ... State_>
struct contained_states<std::tuple<State_...>, void>
{
    using all = tuple_join_t<typename contained_states<State_, base_t<State_>>::all...>;
};

template <typename State_>
using contained_states_direct_t = typename contained_states<State_, base_t<State_>>::direct;

template <typename State_>
using contained_states_recursive_t = typename contained_states<State_, base_t<State_>>::all;

template <typename State_>
using all_states_t = tuple_join_t<State_, contained_states_recursive_t<State_>>;

template <typename State_, typename Config_ = void>
struct top_state {
    using type = 
        std::conditional_t<
            std::is_void_v<typename State_::Conf::TopState>,
            typename State_::template TopStateTemplate<typename State_::Conf>,
            typename State_::Conf::TopState>;
};

template <typename State_>
struct top_state<State_, void>
{
    using type = typename State_::TopState;
};

template <typename State_>
using top_state_t = typename top_state<State_, typename State_::Conf>::type;

template <typename State_>
struct state_id
{
    static constexpr std::size_t value = index_v<State_, all_states_t<top_state_t<State_>>>;
};

template <>
struct state_id<void>
{
    static constexpr std::size_t value = 0;
};

template <typename State_>
constexpr std::size_t state_id_v = state_id<State_>::value;

template <typename State_>
using state_combination_t = uint64_t; //std::bitset<std::tuple_size_v<all_states_t<top_state_t<State_>>>>;
static_assert(sizeof(uint64_t) == 8);
template <typename State_>
constexpr auto state_combination(type_identity<State_>)
{
    state_combination_t<top_state_t<State_>> value = (uint64_t{1} << state_id_v<State_>);
    //value |= (1 << state_id_v<State_>);
    return value;
};

template <typename State1_, typename ... State_>
constexpr auto state_combination(type_identity<std::tuple<State1_, State_...>>)
{
    if constexpr(sizeof...(State_) > 0) {
        return (uint64_t{1} << state_id_v<State1_>) | ((uint64_t{1} << state_id_v<State_>) | ...);
    }
    else {
        return (uint64_t{1} << state_id_v<State1_>);
    }
};

template <typename State_>
constexpr auto state_combination_recursive_v = state_combination(type_identity<all_states_t<State_>>{});

template <typename State_>
constexpr auto state_combination_v = state_combination(type_identity<State_>{});

template <typename State1_, typename StateTuple_>
struct any;

template <typename State1_, typename ... State_>
struct any<State1_, std::tuple<State_...>>
{
    static constexpr bool value = ((state_id_v<State1_> == state_id_v<State_>) || ...);
};

template <typename State_, typename SuperState_, typename OtherStates_>
struct super_state_impl;

template <typename State_, typename OtherStates_>
struct super_state;

template <typename State_, typename SuperState_, typename ... OtherState_>
struct super_state_impl<State_, std::tuple<SuperState_>, std::tuple<OtherState_...>>
{
    using direct = SuperState_;
    using recursive = tuple_join_t<direct, typename super_state<direct, std::tuple<OtherState_...>>::recursive>;
};

template <typename State_, typename ... OtherState_>
struct super_state_impl<State_, std::tuple<>, std::tuple<OtherState_...>>
{
    using direct = void;
    using recursive = std::tuple<>;
};

template <typename State_, typename ... OtherState_>
struct super_state<State_, std::tuple<OtherState_...>>
{
    using impl = super_state_impl<State_, tuple_strip_void_t<std::tuple<
        std::conditional_t<
           any<State_, contained_states_direct_t<OtherState_>>::value,
            OtherState_,
            void>...
        >>, std::tuple<OtherState_...>>;
    using direct = typename impl::direct;
    using recursive = typename impl::recursive;
};

template <typename State_>
using super_state_direct_t  = typename super_state<State_, all_states_t<top_state_t<State_>>>::direct;

template <typename State_>
using super_state_recursive_t  = typename super_state<State_, all_states_t<top_state_t<State_>>>::recursive;

template <typename State_, typename StateBase_ = base_t<State_>>
struct default_initial_state;

template <typename State_, bool has_initial = has_initial_v<State_>>
struct initial_state
{
    using type = typename State_::Initial;
};

template <typename State_>
struct initial_state<State_, false>
{
    using type = typename default_initial_state<State_>::type;
};

template <typename Regions_>
struct initial_states;

template <typename ... Region_>
struct initial_states<std::tuple<Region_...>>
{
    using type = std::tuple<typename initial_state<Region_>::type...>;
};

template <typename State_>
struct default_initial_state<State_, CompositeStateBase>
{
    using type = std::tuple_element_t<0, typename State_::SubStates>;
};

template <typename State_>
struct default_initial_state<State_, OrthogonalStateBase>
{
    using type = typename initial_states<typename State_::Regions>::type;
};

template <typename State_>
using initial_state_t = typename initial_state<State_>::type;

template <typename State_>
class SimpleStateWrapper;

template <typename State_>
class CompositeStateWrapper;

template <typename State_>
class OrthogonalStateWrapper;

template <typename State_, typename StateBase_ = base_t<State_>>
struct wrapper;

template <typename State_>
struct wrapper<State_, SimpleStateBase> { using type = SimpleStateWrapper<State_>; };

template <typename State_>
struct wrapper<State_, CompositeStateBase> { using type = CompositeStateWrapper<State_>; };

template <typename State_>
struct wrapper<State_, OrthogonalStateBase> { using type = OrthogonalStateWrapper<State_>; };

template <typename State_>
using wrapper_t = typename wrapper<State_>::type;

template <typename State_>
struct is_orthogonal_state
{
    static constexpr bool value = std::is_same_v<base_t<State_>, OrthogonalStateBase>;
};

template <typename States_>
using orthogonal_states_t = tuple_filter_t<is_orthogonal_state, States_>;

template <typename OrthogonalStates_>
struct all_regions;

template <typename ... OrthogonalState_>
struct all_regions<std::tuple<OrthogonalState_...>>
{
    using type = tuple_join_t<typename OrthogonalState_::Regions ...>;
};

template <typename TopState_>
using all_regions_t = tuple_join_t<TopState_, typename all_regions<orthogonal_states_t<all_states_t<TopState_>>>::type>;

template <typename Region_, typename RegionNext_, typename ... RegionRest_>
constexpr auto region_mask(type_identity<std::tuple<RegionNext_, RegionRest_...>>)
{
    if constexpr(!std::is_same_v<Region_, RegionNext_>) {
        return region_mask<Region_>(type_identity<std::tuple<RegionRest_...>>{});
    }
    if constexpr(sizeof...(RegionRest_) > 0) {
        return state_combination_recursive_v<Region_>
            & (~state_combination_recursive_v<RegionRest_> & ...);
    }
    else {
        return state_combination_recursive_v<Region_>;
    }
};

template <typename TopState_>
using RegionMasks = std::array<std::size_t, std::tuple_size_v<all_regions_t<TopState_>>>;

template <typename TopState_, typename ... Region_>
constexpr RegionMasks<TopState_> region_masks(type_identity<std::tuple<Region_...>>)
{
    return {region_mask<Region_>(type_identity<std::tuple<Region_...>>{})...};
};

template <typename TopState_>
constexpr RegionMasks<TopState_> region_masks_v 
    = region_masks<TopState_>(type_identity<all_regions_t<TopState_>>{});


template <typename TopState_>
constexpr bool is_valid(state_combination_t<TopState_> const& c1, state_combination_t<TopState_> const& c2) {
    constexpr auto& masks = region_masks_v<TopState_>;
    return !c1 || !c2 || std::apply([&](auto& ... mask) {
        return ((!((c1 & ~c2) & mask) || !((c2 & ~c1) & mask)) || ...);
    }, masks);
}

}