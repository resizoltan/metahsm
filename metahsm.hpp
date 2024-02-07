#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>
#include <trace.hpp>

#include "type_traits.hpp"

namespace metahsm {

template <typename _StateDef>
class StateMixin;

//=====================================================================================================//
//                                     STATE TEMPLATE - USER API                                       //
//=====================================================================================================//

template <typename _StateDef, typename _TopStateDef>
class StateCrtp : public StateBase
{
public:
    using StateDef = _StateDef;
    using TopStateDef = _TopStateDef;

    template <typename _SubStateDef>
    using State = StateCrtp<_SubStateDef, TopStateDef>;

    template <typename _ContextDef>
    decltype(auto) context()  {
        //static_assert(is_in_context_recursive_v<_StateDef, _ContextDef>);
        return this->mixin().template context<_ContextDef>();
    }

    template <typename _TargetStateDef>
    auto transition() {
        //(static_assert(is_in_context_recursive_v<_TargetStateDef, TopStateDef>),...);
        this->mixin().template set_transition<_TargetStateDef>();
    }

public:
    decltype(auto) mixin() {
        return static_cast<StateMixin<_StateDef>&>(*this);
    }

    static TopStateDef top_state_def();
};

//=====================================================================================================//
//                    STATE MIXINS - INTERNAL, IMPLEMENT STATE MACHINE BEHAVIOR                        //
//=====================================================================================================//

template <typename _TopStateDef>
class StateMachine;

template <typename _StateDef>
class StateMixin : public _StateDef
{
public:
    using StateDef = _StateDef;
    using TopStateDef = decltype(_StateDef::top_state_def());
    using SuperStateDef = super_state_t<_StateDef>;
    using SuperStateMixin = StateMixin<SuperStateDef>;
    using ContainedStates = mixins_t<contained_states_direct_t<_StateDef>>;

    StateMixin(SuperStateMixin& super_state_mixin)
    : StateMixin(super_state_mixin, std::make_index_sequence<std::tuple_size_v<ContainedStates>>())
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        this->target_combination_ = 0;
        if constexpr(has_reaction_to_event_v<_StateDef, _Event>) {
            const auto result = std::make_tuple(this->react(e), this->target_combination_);
            trace_react<_StateDef>(result);
            return result;
        }
        else {
            return std::make_tuple(false, (std::size_t)0);
        }
    }

    template <typename _ContextDef>
    auto& context() {
        if constexpr(std::is_same_v<_ContextDef, _StateDef>) {
            return *this;
        }
        else {
            return super_state_mixin_.template context<_ContextDef>();
        }
    }

    template <typename _TargetStateDef>
    void set_transition() {
        target_combination_ = state_combination_v<_TargetStateDef>;
    }

    template <typename _ContainedState>
    StateMixin<_ContainedState>& get_contained_state() {
        return std::get<StateMixin<_ContainedState>>(contained_states_);
    }

protected:
    template <std::size_t ... i>
    StateMixin(SuperStateMixin& super_state_mixin, std::index_sequence<i...>)
    : super_state_mixin_{super_state_mixin},
      contained_states_{(i, *this)...} 
    {}

    SuperStateMixin& super_state_mixin_;
    ContainedStates contained_states_;
    std::size_t target_combination_;
};

template <>
struct StateMixin<RootState>
{};

template <typename _TopStateDef>
class TopState : public StateCrtp<_TopStateDef, _TopStateDef> , public TopStateBase
{};

template <typename _StateMixin>
class StateWrapper {
public:
    using StateDef = typename _StateMixin::StateDef;

    StateWrapper(_StateMixin& state)
    : state_{state}
    {
        trace_enter<StateDef>();
        if constexpr (has_entry_action_v<StateDef>) {
            state_.on_entry();
        }
    }

    ~StateWrapper()
    {
        trace_exit<StateDef>();
        if constexpr (has_exit_action_v<StateDef>) {
            state_.on_exit();
        }
    }

protected:
    _StateMixin& state_;
};

template <typename _StateMixin>
class SimpleStateWrapper : public StateWrapper<_StateMixin>
{
public:
    SimpleStateWrapper(_StateMixin& state, std::size_t target_combination)
    : StateWrapper<_StateMixin>(state)
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        return this->state_.handle_event(e);
    }

    void execute_transition(std::size_t) { }
};

template <typename _StateMixin>
class CompositeStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using SubStates = typename StateDef::SubStates;
    using SubStateMixins = mixins_t<SubStates>;

    CompositeStateWrapper(_StateMixin& state, std::size_t target_combination)
    : StateWrapper<_StateMixin>(state)
    {
        std::size_t substate_to_enter_local_id = compute_direct_substate(target_combination, type_identity<SubStates>{});
        std::invoke(lookup_table[substate_to_enter_local_id], this, target_combination);
    }

    template <typename _Event>
    auto handle_event(const _Event& e) {
        auto do_handle_event = overload{
            [&](auto& active_sub_state){ return active_sub_state.handle_event(e); },
            [](std::monostate){ return std::make_tuple(false, (std::size_t)0); }
        };
        auto [substate_reacted, transition] = std::visit(do_handle_event, active_sub_state_);
        return substate_reacted ? std::make_tuple(substate_reacted, transition) : this->state_.handle_event(e);
    }

    void execute_transition(std::size_t target_combination) {
        remove_conflicting(target_combination, type_identity<SubStates>{});
        bool is_target_in_context = static_cast<bool>(target_combination & (1 << active_state_id_))
            || (!static_cast<bool>(target_combination & state_combination_v<SubStates>) && initial_state_id_ == active_state_id_);

        if(is_target_in_context) {
            auto do_execute_transition = overload{
                [&](auto& active_sub_state){ active_sub_state.execute_transition(target_combination); },
                [](std::monostate) { }
            };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            std::size_t substate_to_enter_local_id = compute_direct_substate(target_combination, type_identity<SubStates>{});
            std::invoke(lookup_table[substate_to_enter_local_id], this, target_combination);
        }
    }

private:
    template <typename ... _SubStateDef>
    static constexpr auto init_lookup_table(type_identity<std::tuple<_SubStateDef...>>) {
        return std::array{&enter_substate<_SubStateDef>..., &enter_substate<initial_state_t<StateDef>>};
    }

    template <typename _DirectSubStateToEnter>
    void enter_substate(std::size_t target_combination){
        using T = wrapper_t<StateMixin<_DirectSubStateToEnter>>;
        active_sub_state_.template emplace<T>(this->state_.template get_contained_state<_DirectSubStateToEnter>(), target_combination);
        active_state_id_ = state_id_v<_DirectSubStateToEnter>;
    }

    to_variant_t<tuple_join_t<std::monostate, wrappers_t<SubStateMixins>>> active_sub_state_;
    std::size_t active_state_id_;
    static constexpr std::size_t initial_state_id_ = state_id_v<initial_state_t<StateDef>>;
    static constexpr std::array<void(CompositeStateWrapper<StateMixin<StateDef>>::*)(std::size_t), std::tuple_size_v<SubStates> + 1> lookup_table = init_lookup_table(type_identity<SubStates>{});
};

template <typename _StateMixin>
class OrthogonalStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using Regions = typename StateDef::Regions;
    using RegionMixins = mixins_t<Regions>;

    OrthogonalStateWrapper(_StateMixin& state, std::size_t target_combination)
    : OrthogonalStateWrapper<_StateMixin>(state, target_combination, type_identity<Regions>{})
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        auto do_handle_event = [&](auto& ... region){ return (region.handle_event(e) + ...); };
        auto [substate_reacted, transition] = std::apply(do_handle_event, regions_);
        return substate_reacted ? std::make_tuple(substate_reacted, transition) : this->state_.handle_event(e);
    }

    void execute_transition(std::size_t target_combination) {
        auto do_execute_transition = [&](auto& ... region){ (region.execute_transition(target_combination), ...); };
        std::apply(do_execute_transition, regions_);
    }

private:
    template <typename ... RegionDef>
    OrthogonalStateWrapper(_StateMixin& state, std::size_t target_combination, type_identity<std::tuple<RegionDef...>>)
    : StateWrapper<_StateMixin>(state),
      regions_{wrapper_t<StateMixin<RegionDef>>{state.template get_contained_state<RegionDef>(), target_combination}...} 
    {}

    wrappers_t<RegionMixins> regions_;
};

//=====================================================================================================//
//                                         STATE MACHINE                                               //
//=====================================================================================================//

template <typename _TopStateDef>
class StateMachine
{
public:
    StateMachine()
    : top_state_{root_state_mixin_},
      active_state_configuration_{top_state_, state_combination_v<initial_state_t<_TopStateDef>>}
    {}

    template <typename _Event>
    bool dispatch(const _Event& event) {
        auto [any_state_reacted, target_combination] = active_state_configuration_.handle_event(event);
        if(any_state_reacted && target_combination != 0) {
            active_state_configuration_.execute_transition(target_combination);
        }
        return any_state_reacted;
    }

private:
    StateMixin<_TopStateDef> top_state_;
    wrapper_t<StateMixin<_TopStateDef>> active_state_configuration_;
    StateMixin<RootState> root_state_mixin_;
};



}