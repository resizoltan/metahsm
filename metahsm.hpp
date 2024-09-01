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

struct ReactionResult
{
    bool reacted{false};
    std::size_t target_combination_initial{0};
    std::size_t target_combination_shallow{0};
    std::size_t target_combination_deep{0};
};

auto operator+(ReactionResult lhs, ReactionResult rhs) {
    return ReactionResult{
        lhs.reacted || rhs.reacted,
        lhs.target_combination_initial | rhs.target_combination_initial,
        lhs.target_combination_shallow | rhs.target_combination_shallow,
        lhs.target_combination_deep | rhs.target_combination_deep};
}

template <typename _StateDef>
void trace_react(ReactionResult result);

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
    using SuperStateDef = super_state_t<_StateDef>;
    using SuperStateMixin = StateMixin<SuperStateDef>;
    using ContainedStates = mixins_t<contained_states_direct_t<_StateDef>>;

    StateMixin(SuperStateMixin& super_state_mixin)
    : StateMixin(super_state_mixin, std::make_index_sequence<std::tuple_size_v<ContainedStates>>())
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
        if constexpr(std::is_same_v<_ContextDef, _StateDef>) {
            return *this;
        }
        else {
            return super_state_mixin_.template context<_ContextDef>();
        }
    }

    template <typename _TargetStateDef>
    void set_transition() {
        if constexpr(std::is_base_of_v<HistoryBase<DEEP>, _TargetStateDef>) {
            reaction_result_.target_combination_deep = state_combination_v<typename _TargetStateDef::TargetStateDef>;
        }
        else if constexpr(std::is_base_of_v<HistoryBase<SHALLOW>, _TargetStateDef>) {
            reaction_result_.target_combination_shallow = state_combination_v<typename _TargetStateDef::TargetStateDef>;
        }
        else {
            reaction_result_.target_combination_initial = state_combination_v<_TargetStateDef>;
        }
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
    ReactionResult reaction_result_;
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
        if(moved_from) {
            return;
        }
        trace_exit<StateDef>();
        if constexpr (has_exit_action_v<StateDef>) {
            state_.on_exit();
        }
    }

    StateWrapper(StateWrapper&& other)
    : state_{other.state_}
    {
        other.moved_from = true;
    }

protected:
    _StateMixin& state_;
    bool moved_from{false};
};

template <typename _StateMixin>
class SimpleStateWrapper : public StateWrapper<_StateMixin>
{
public:
    SimpleStateWrapper(_StateMixin& state, std::size_t initial, std::size_t shallow, std::size_t deep)
    : StateWrapper<_StateMixin>(state)
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        return this->state_.handle_event(e);
    }

    void execute_transition(std::size_t, std::size_t, std::size_t) { }
};

template <typename _StateMixin>
class CompositeStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using SubStates = typename StateDef::SubStates;
    using SubStateMixins = mixins_t<SubStates>;

    CompositeStateWrapper(_StateMixin& state, std::size_t initial, std::size_t shallow, std::size_t deep)
    : StateWrapper<_StateMixin>(state)
    {
        std::size_t target = initial | shallow | deep;
        std::size_t substate_to_enter_local_id = compute_direct_substate<typename _StateMixin::StateDef>(active_state_local_id_, initial, shallow, deep, type_identity<SubStates>{});
        std::invoke(lookup_table[substate_to_enter_local_id], this, initial, shallow, deep);
    }

    template <typename _Event>
    auto handle_event(const _Event& e) {
        auto do_handle_event = overload{
            [&](auto& active_sub_state){ return active_sub_state.handle_event(e); },
            [](std::monostate){ return ReactionResult{}; }
        };
        auto reaction_result = std::visit(do_handle_event, active_sub_state_);
        return reaction_result.reacted ? reaction_result : this->state_.handle_event(e);
    }

    void execute_transition(std::size_t initial, std::size_t shallow, std::size_t deep) {
        remove_conflicting(initial, shallow, deep, type_identity<SubStates>{});
        std::size_t target = initial | shallow | deep;
        bool is_target_in_context = (
            static_cast<bool>((target & (1 << active_state_id_)))
        )
        || (
            !static_cast<bool>(target & state_combination_v<SubStates>) && initial_state_id_ == active_state_id_
        );

        if(is_target_in_context) {
            auto do_execute_transition = overload{
                [&](auto& active_sub_state){ active_sub_state.execute_transition(initial, shallow, deep); },
                [](std::monostate) { }
            };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            std::size_t substate_to_enter_local_id = compute_direct_substate<typename _StateMixin::StateDef>(active_state_local_id_, initial, shallow, deep, type_identity<SubStates>{});
            std::invoke(lookup_table[substate_to_enter_local_id], this, initial, shallow, deep);
        }
    }

private:
    template <typename ... _SubStateDef>
    static constexpr auto init_lookup_table(type_identity<std::tuple<_SubStateDef...>>) {
        return std::array{&enter_substate<_SubStateDef>..., &enter_substate<initial_state_t<StateDef>>};
    }

    template <typename _DirectSubStateToEnter>
    void enter_substate(std::size_t initial, std::size_t shallow, std::size_t deep){
        using T = wrapper_t<StateMixin<_DirectSubStateToEnter>>;
        active_sub_state_.template emplace<T>(this->state_.template get_contained_state<_DirectSubStateToEnter>(), initial, shallow, deep);
        active_state_id_ = state_id_v<_DirectSubStateToEnter>;
        active_state_local_id_ = index_v<_DirectSubStateToEnter, SubStates>;
    }

    to_variant_t<tuple_join_t<std::monostate, wrappers_t<SubStateMixins>>> active_sub_state_;
    std::size_t active_state_id_ = state_id_v<initial_state_t<StateDef>>;
    std::size_t active_state_local_id_ = index_v<initial_state_t<StateDef>, SubStates>;
    static constexpr std::size_t initial_state_id_ = state_id_v<initial_state_t<StateDef>>;
    static constexpr std::array<void(CompositeStateWrapper<StateMixin<StateDef>>::*)(std::size_t, std::size_t, std::size_t), std::tuple_size_v<SubStates> + 1> lookup_table = init_lookup_table(type_identity<SubStates>{});
};

template <typename _StateMixin>
class OrthogonalStateWrapper : public StateWrapper<_StateMixin>
{
public:
    using typename StateWrapper<_StateMixin>::StateDef;
    using Regions = typename StateDef::Regions;
    using RegionMixins = mixins_t<Regions>;

    OrthogonalStateWrapper(_StateMixin& state, std::size_t initial, std::size_t shallow, std::size_t deep)
    : OrthogonalStateWrapper<_StateMixin>(state, initial, shallow, deep, type_identity<Regions>{})
    {}

    template <typename _Event>
    auto handle_event(const _Event& e) {
        auto do_handle_event = [&](auto& ... region){ return (region.handle_event(e) + ...); };
        auto reaction_result = std::apply(do_handle_event, regions_);
        return reaction_result.reacted ? reaction_result : this->state_.handle_event(e);
    }

    void execute_transition(std::size_t initial, std::size_t shallow, std::size_t deep) {
        auto do_execute_transition = [&](auto& ... region){ (region.execute_transition(initial, shallow, deep), ...); };
        std::apply(do_execute_transition, regions_);
    }

private:
    template <typename ... RegionDef>
    OrthogonalStateWrapper(_StateMixin& state, std::size_t initial, std::size_t shallow, std::size_t deep, type_identity<std::tuple<RegionDef...>>)
    : StateWrapper<_StateMixin>(state),
      regions_{std::move(wrapper_t<StateMixin<RegionDef>>{state.template get_contained_state<RegionDef>(), initial, shallow, deep})...} 
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
      active_state_configuration_{top_state_, state_combination_v<initial_state_t<_TopStateDef>>, 0, 0}
    {}

    template <typename _Event>
    bool dispatch(const _Event& event) {
        auto reaction_result = active_state_configuration_.handle_event(event);
        if(reaction_result.reacted
            && (reaction_result.target_combination_initial
              | reaction_result.target_combination_shallow
              | reaction_result.target_combination_deep
            ) != 0
        ) {
            active_state_configuration_.execute_transition(
                reaction_result.target_combination_initial,
                reaction_result.target_combination_shallow,
                reaction_result.target_combination_deep);
        }
        return reaction_result.reacted;
    }

private:
    StateMixin<_TopStateDef> top_state_;
    wrapper_t<StateMixin<_TopStateDef>> active_state_configuration_;
    StateMixin<RootState> root_state_mixin_;
};



}

#include "trace.hpp"
