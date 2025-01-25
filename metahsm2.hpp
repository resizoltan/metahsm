#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>
#include <optional>

#include "type_traits.hpp"

namespace metahsm {

//=====================================================================================================//
//                                     STATE TEMPLATE - USER API                                       //
//=====================================================================================================//

template <typename TopState_>
class StateMachine;

namespace detail {

template <typename TopState_>
class State_ : public StateBase
{
public:
  using State = State_<TopState_>;
  using TopState = TopState_;

protected:

  template <typename TargetState_>
  void transition() {
      //(static_assert(is_in_context_recursive_v<_TargetStateDef, TopStateDef>),...);
    static_cast<state_combination_t<TopState_>*>(target_state_combination_)->set(state_id_v<TargetState_>);
  }

public:
  void * target_state_combination_; // evaluation of state_combination_t needs to be deferred
};
}

template <typename TopState_>
using State = detail::State_<TopState_>;


template <typename State_>
class StateWrapper {
public:
  using StateMachine = metahsm::StateMachine<typename State_::TopState>;

  StateWrapper(State_& state)
  : state_{state}
  {
    if constexpr (has_entry_action_v<State_>) {
      state_.on_entry();
    }
  }

  ~StateWrapper()
  {
    if constexpr (has_exit_action_v<State_>) {
      state_.on_exit();
    }
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    if constexpr(has_reaction_to_event_v<State_, Event_>) {
      if constexpr(std::is_void_v<decltype(this->state_.react(e))>) {
        this->state_.react(e);
        return true;
      }
      else {
        return this->state_.react(e);
      }
    }
    return false; 
  }

protected:
  State_ & state_;
};

template <typename State_>
class SimpleStateWrapper : public StateWrapper<State_>
{
public:
  using typename StateWrapper<State_>::StateMachine;

  SimpleStateWrapper(State_& state, StateMachine&)
  : StateWrapper<State_>(state)
  { }
};

template <typename State_>
class CompositeStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = typename State_::TopState;
  using SubStates = typename State_::SubStates;
  using SubStateWrappers = tuple_apply_t<wrapper_t, SubStates>;
  using typename StateWrapper<State_>::StateMachine;

  CompositeStateWrapper(State_& state, StateMachine& state_machine)
  : StateWrapper<State_>(state),
    state_machine_{state_machine},
    active_sub_state_{std::in_place_type<wrapper_t<initial_state_t<State_>>>,
      state_machine.template get_state<initial_state_t<State_>>(),
      state_machine_},
    active_state_combination_{state_combination_recursive_v<initial_state_t<State_>>}
  { }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    auto do_handle_event = [&](auto& active_sub_state){
      return active_sub_state.handle_event(e); 
    };
    bool reacted = std::visit(do_handle_event, active_sub_state_);
    return reacted | this->StateWrapper<State_>::handle_event(e);
  }

  void execute_transition(state_combination_t<TopState> const& target) {
    if ((target & active_state_combination_).none()) {
      change_state(target, type_identity<SubStates>{});
    }
  }

private:  
  StateMachine& state_machine_;
  to_variant_t<SubStateWrappers> active_sub_state_;
  state_combination_t<TopState> active_state_combination_;

  template <typename ... SubState>
  void change_state(state_combination_t<TopState> const& target, type_identity<std::tuple<SubState...>>) {
    bool changed = ((
      (state_combination_recursive_v<SubState> & target).any() ? 
        (
          active_sub_state_.template emplace<wrapper_t<SubState>>(state_machine_.template get_state<SubState>(), state_machine_),
          active_state_combination_ = state_combination_recursive_v<SubState>,
          true
        ) : false
    ) || ...);
  }

};


//=====================================================================================================//
//                                         STATE MACHINE                                               //
//=====================================================================================================//


template <typename TopState_>
class StateMachine
{
public:
  using States = all_states_t<TopState_>;
  static constexpr std::size_t N = std::tuple_size_v<States>;

  StateMachine()
  : active_state_configuration_{std::get<TopState_>(all_states_), *this}
  {
    init(std::make_index_sequence<N>());
  }

  // TODO copy, move ctor

  template <typename Event_>
  bool dispatch(const Event_& event = {}) {
    bool reacted = active_state_configuration_.handle_event(event);
    auto target = compute_target_state_combination();
    active_state_configuration_.execute_transition(target);
    return reacted;
  }

  template <typename State_>
  auto& get_state() {
    return std::get<State_>(all_states_);
  }

  template <typename State_>
  auto& get_target_state_combination() {
    return std::get<state_id_v<State_>>(target_state_combinations_);
  }

  auto compute_target_state_combination() {
    auto it = std::find_if(target_state_combinations_.begin(), target_state_combinations_.end(), [&](auto& v){ return v.any(); });
    state_combination_t<TopState_> target{};
    if (it != target_state_combinations_.end()) {
      target = *it;
      for(auto & comb : target_state_combinations_) {
        comb = {};
      }
    }
    return target;
  }

private:
  template <auto ... I>
  void init(std::index_sequence<I...>) {
    ((std::get<I>(all_states_).target_state_combination_ = &std::get<I>(target_state_combinations_)), ...);
  }

  States all_states_;
  wrapper_t<TopState_> active_state_configuration_;
  std::array<state_combination_t<TopState_>, N> target_state_combinations_;
};


}
