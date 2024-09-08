#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>
#include <optional>

#include "type_traits.hpp"

namespace metahsm {

template <typename _StateDef>
class StateMixin;

enum HistoryType {
    DEEP,
    SHALLOW,
    NONE
};

template <HistoryType type>
struct HistoryBase
{};

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

private:
    decltype(auto) mixin() {
        return static_cast<StateMixin<_StateDef>&>(*this);
    }

    friend class StateMixin<_StateDef>;
    static TopStateDef top_state_def();
};

template <typename _TargetStateDef, HistoryType type = SHALLOW>
struct History : public HistoryBase<type>
{
    using TargetStateDef = _TargetStateDef;
    static constexpr HistoryType TYPE = type;
};

//=====================================================================================================//
//               STATE MIXINS & WRAPPERS - INTERNAL, IMPLEMENT STATE MACHINE BEHAVIOR                  //
//=====================================================================================================//

template <typename _TopStateDef>
class StateMachine;

template <typename _TopState>
struct ReactionResult
{
    bool reacted{false};
    std::size_t target_combination{0};
};

template <typename _TopState>
auto operator,(ReactionResult<_TopState> lhs, ReactionResult<_TopState> rhs) {
    std::size_t combined_target_combination = is_legal_state_combination<_TopState>(lhs.target_combination | rhs.target_combination) ?
        lhs.target_combination | rhs.target_combination : lhs.target_combination;
    return ReactionResult<_TopState>{lhs.reacted || rhs.reacted, combined_target_combination};
}

template <typename _StateDef>
void trace_react(ReactionResult<typename _StateDef::TopStateDef> result);

template <typename _StateDef>
void trace_enter();

template <typename _StateDef>
void trace_exit();

template <typename _StateDef>
class StateMixin : public _StateDef
{
public:
    using StateDef = _StateDef;
    using TopStateDef = decltype(_StateDef::top_state_def());
    using ContainedStates = mixins_t<contained_states_direct_t<_StateDef>>;
    using _StateMachine = StateMachine<TopStateDef>;

    StateMixin(_StateMachine& state_machine)
    : state_machine_{state_machine}
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        reaction_result_ = {};
        if constexpr(has_reaction_to_event_v<_StateDef, _Event>) {
            reaction_result_.reacted = this->react(e);
            trace_react<_StateDef>(reaction_result_);
            if(!reaction_result_.reacted) {
                reaction_result_ = {};
            }
        }
        return reaction_result_;
    }

    template <typename _ContextDef>
    auto& context() {
        return state_machine_.template get_context<_ContextDef>();
    }

    template <typename _TargetStateDef>
    void set_transition() {
        if constexpr(std::is_base_of_v<HistoryBase<DEEP>, _TargetStateDef>) {
            reaction_result_.target_combination = state_machine_.template get_deep_history<typename _TargetStateDef::TargetStateDef>()
                | state_combination_v<typename _TargetStateDef::TargetStateDef>
                | state_combination_v<super_state_recursive_t<typename _TargetStateDef::TargetStateDef>>;
        }
        else if constexpr(std::is_base_of_v<HistoryBase<SHALLOW>, _TargetStateDef>) {
            reaction_result_.target_combination = state_machine_.template get_shallow_history<typename _TargetStateDef::TargetStateDef>()
                | state_combination_v<typename _TargetStateDef::TargetStateDef>
                | state_combination_v<super_state_recursive_t<typename _TargetStateDef::TargetStateDef>>;
        }
        else {
            reaction_result_.target_combination = initial_state_combination_v<_TargetStateDef>
                | state_combination_v<_TargetStateDef>
                | state_combination_v<super_state_recursive_t<_TargetStateDef>>;
        }
    }

    void set_current_state_combination(std::size_t state_combination) {
        current_state_combination_ = state_combination;
    }

    std::size_t get_current_state_combination() {
        return current_state_combination_;
    }

protected:
    _StateMachine& state_machine_;
    ReactionResult<TopStateDef> reaction_result_;
    std::size_t current_state_combination_;
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
    using TopStateDef = typename _StateMixin::TopStateDef;
    using _StateMachine = StateMachine<TopStateDef>;

    StateWrapper(_StateMixin& state, _StateMachine& state_machine)
    : state_{state},
      state_machine_{state_machine}
    {
        trace_enter<StateDef>();
        if constexpr (has_entry_action_v<StateDef>) {
            state_.on_entry();
        }
    }

    ~StateWrapper()
    {
        if(moved_from) {
            return;
        }
        trace_exit<StateDef>();
        if constexpr (has_exit_action_v<StateDef>) {
            state_.on_exit();
        }
    }

    StateWrapper(StateWrapper&& other)
    : state_{other.state_},
      state_machine_{other.state_machine_}
    {
        other.moved_from = true;
    }

    

protected:
    _StateMixin& state_;
    _StateMachine& state_machine_;
    bool moved_from{false};
};

template <typename _StateMixin>
class SimpleStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using typename StateWrapper<_StateMixin>::TopStateDef;
    using typename StateWrapper<_StateMixin>::_StateMachine;

    SimpleStateWrapper(_StateMixin& state, std::size_t, _StateMachine& state_machine)
    : StateWrapper<_StateMixin>(state, state_machine)
    {
        this->state_.set_current_state_combination(get_current_state_combination());    
    }

    template <typename _Event>
    auto handle_event(const _Event& e) {
        return this->state_.handle_event(e);
    }

    void execute_transition(std::size_t) { }

    std::size_t get_current_state_combination() {
        return state_combination_v<StateDef>;
    }
};

template <typename _StateMixin>
class CompositeStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using typename StateWrapper<_StateMixin>::TopStateDef;
    using typename StateWrapper<_StateMixin>::_StateMachine;
    using SubStates = typename StateDef::SubStates;
    using SubStateMixins = mixins_t<SubStates>;
    using LookupTable = std::array<void(CompositeStateWrapper<StateMixin<StateDef>>::*)(std::size_t), std::tuple_size_v<SubStates> + 1>;

    CompositeStateWrapper(_StateMixin& state, std::size_t target, _StateMachine& state_machine)
    : StateWrapper<_StateMixin>(state, state_machine)
    {
        std::size_t substate_to_enter_local_id = compute_direct_substate<typename _StateMixin::StateDef>(target, type_identity<SubStates>{});
        std::invoke(lookup_table[substate_to_enter_local_id], this, target);
        this->state_.set_current_state_combination(get_current_state_combination());
    }

    template <typename _Event>
    auto handle_event(const _Event& e) {
        auto do_handle_event = overload{
            [&](auto& active_sub_state){ return active_sub_state.handle_event(e); },
            [](std::monostate){ return ReactionResult<TopStateDef>{}; }
        };
        auto reaction_result = std::visit(do_handle_event, active_sub_state_);
        return reaction_result.reacted ? reaction_result : this->state_.handle_event(e);
    }

    void execute_transition(std::size_t target) {
        if(!static_cast<bool>(target & state_combination_v<SubStates>) && initial_state_id_ == active_state_id_) {
            return; 
        }

        if(static_cast<bool>(target & (1 << active_state_id_))) {
            auto do_execute_transition = overload{
                [&](auto& active_sub_state){ active_sub_state.execute_transition(target); },
                [](std::monostate) { }
            };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            std::size_t substate_to_enter_local_id = compute_direct_substate<typename _StateMixin::StateDef>(target, type_identity<SubStates>{});
            std::invoke(lookup_table[substate_to_enter_local_id], this, target);
        }
    }

    std::size_t get_current_state_combination() {
        auto do_get_state_combination = overload{
            [&](auto& active_substate) { return active_substate.get_current_state_combination(); },
            [](std::monostate){ return std::size_t{0}; }
        };
        return std::visit(do_get_state_combination, active_sub_state_) | state_combination_v<StateDef>;
    }

private:
    template <typename ... _SubStateDef>
    static auto init_lookup_table(type_identity<std::tuple<_SubStateDef...>>) {
        return LookupTable{&CompositeStateWrapper<_StateMixin>::enter_substate<_SubStateDef>..., &CompositeStateWrapper<_StateMixin>::enter_substate<initial_state_t<StateDef>>};
    }

    template <typename _DirectSubStateToEnter>
    void enter_substate(std::size_t target){
        using T = wrapper_t<StateMixin<_DirectSubStateToEnter>>;
        active_sub_state_.template emplace<T>(this->state_machine_.template get_state<_DirectSubStateToEnter>(), target, this->state_machine_);
        active_state_id_ = state_id_v<_DirectSubStateToEnter>;
        this->state_.set_current_state_combination(get_current_state_combination());
    }

    to_variant_t<tuple_join_t<std::monostate, wrappers_t<SubStateMixins>>> active_sub_state_;
    std::size_t active_state_id_ = state_id_v<initial_state_t<StateDef>>;
    static constexpr std::size_t initial_state_id_ = state_id_v<initial_state_t<StateDef>>;
    static constexpr LookupTable lookup_table = init_lookup_table(type_identity<SubStates>{});
};

template <typename _StateMixin>
class OrthogonalStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using typename StateWrapper<_StateMixin>::TopStateDef;
    using typename StateWrapper<_StateMixin>::_StateMachine;
    using Regions = typename StateDef::Regions;
    using RegionMixins = mixins_t<Regions>;

    OrthogonalStateWrapper(_StateMixin& state, std::size_t target, _StateMachine& state_machine)
    : OrthogonalStateWrapper<_StateMixin>(state, target, type_identity<Regions>{}, state_machine)
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        auto do_handle_event = [&](auto& ... region){ return (region.handle_event(e), ...); };
        auto reaction_result = std::apply(do_handle_event, regions_);
        return reaction_result.reacted ? reaction_result : this->state_.handle_event(e);
    }

    void execute_transition(std::size_t target) {
        auto do_execute_transition = [&](auto& ... region){ (region.execute_transition(target), ...); };
        std::apply(do_execute_transition, regions_);
    }

    std::size_t get_current_state_combination() {
        auto do_get_state_combination = [&](auto& ... region) { return (region.get_current_state_combination() | ...); };
        return std::apply(do_get_state_combination, regions_) | state_combination_v<StateDef>;
    }

private:
    template <typename ... RegionDef>
    OrthogonalStateWrapper(_StateMixin& state, std::size_t target, type_identity<std::tuple<RegionDef...>>, _StateMachine& state_machine)
    : StateWrapper<_StateMixin>(state, state_machine),
      regions_{std::move(wrapper_t<StateMixin<RegionDef>>{state_machine.template get_state<RegionDef>(), target, this->state_machine_})...} 
    {
        this->state_.set_current_state_combination(get_current_state_combination());
    }

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
    : StateMachine(std::make_index_sequence<std::tuple_size_v<all_states_t<_TopStateDef>>>{})
    {}

    template <typename _Event>
    bool dispatch(const _Event& event) {
        auto reaction_result = active_state_configuration_.handle_event(event);
        if(reaction_result.reacted && reaction_result.target_combination != 0
        ) {
            active_state_configuration_.execute_transition(reaction_result.target_combination);
            active_state_combination_ = active_state_configuration_.get_current_state_combination();
        }
        return reaction_result.reacted;
    }

    std::size_t get_active_state_combination() {
        return active_state_combination_;
    }

    template <typename _StateDef>
    StateMixin<_StateDef>& get_state() {
        return std::get<StateMixin<_StateDef>>(all_states_);
    }

    template <typename _ContextDef>
    _ContextDef& get_context() {
        return get_state<_ContextDef>();
    }

    template <typename _StateDef>
    std::size_t get_shallow_history() {
        if constexpr(has_substates_v<_StateDef> || has_regions_v<_StateDef>) {
            return std::get<StateMixin<_StateDef>>(all_states_).get_current_state_combination() & state_combination_v<contained_states_direct_t<_StateDef>>;
        }
        else {
            return state_combination_v<_StateDef>;
        }
    }

    template <typename _StateDef>
    std::size_t get_deep_history() {
        if constexpr(has_substates_v<_StateDef> || has_regions_v<_StateDef>) {
            return std::get<StateMixin<_StateDef>>(all_states_).get_current_state_combination() & state_combination_recursive_v<contained_states_direct_t<_StateDef>>;
        }
        else {
            return state_combination_v<_StateDef>;
        }
    }

private:
    template <auto ... I>
    StateMachine(std::index_sequence<I...>)
    : all_states_{std::move(std::tuple_element_t<I, decltype(all_states_)>{*this})...},
      active_state_configuration_{std::get<0>(all_states_), state_combination_v<initial_state_t<_TopStateDef>>, *this}
    {}

    tuple_apply_t<StateMixin, all_states_t<_TopStateDef>> all_states_;
    wrapper_t<StateMixin<_TopStateDef>> active_state_configuration_;
    std::size_t active_state_combination_;
    StateMixin<RootState> root_state_mixin_;
};



}

#include "trace.hpp"
