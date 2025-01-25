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
class StateMixin : public State_
{
public:
  using typename State_::TopState;
  using StateMachine = metahsm::StateMachine<TopState>;

  /*StateMixin(StateMachine& state_machine)
  : state_machine_{state_machine}
  {}*/

  template <typename Event_>
  bool handle_event(const Event_& e) {
    if constexpr(has_reaction_to_event_v<State_, Event_>) {
      return this->react(e);
    }
    return false;
  }

protected:
  //StateMachine& state_machine_;
};


template <typename State_>
class StateWrapper {
public:
  using StateMachine = metahsm::StateMachine<typename State_::TopState>;

  StateWrapper(StateMixin<State_>& state)
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
    return this->state_.handle_event(e);
  }

protected:
  StateMixin<State_> & state_;
};

template <typename State_>
class SimpleStateWrapper : public StateWrapper<State_>
{
public:
  using typename StateWrapper<State_>::StateMachine;

  SimpleStateWrapper(StateMixin<State_>& state, StateMachine&)
  : StateWrapper<State_>(state)
  { }
};

template <typename State_>
class CompositeStateWrapper : public StateWrapper<State_>
{
public:
  using SubStates = typename State_::SubStates;
  using SubStateMixins = tuple_apply_t<StateMixin, SubStates>;
  using SubStateWrappers = tuple_apply_t<wrapper_t, SubStates>;
  using typename StateWrapper<State_>::StateMachine;

  CompositeStateWrapper(StateMixin<State_>& state, StateMachine& state_machine)
  : StateWrapper<State_>(state),
    state_machine_{state_machine},
    active_sub_state_{std::in_place_type<wrapper_t<initial_state_t<State_>>>,
      state_machine.template get_state<initial_state_t<State_>>(),
      state_machine_}
  { }

  template <typename Event_>
  auto handle_event(const Event_& e) {
    auto do_handle_event = [&](auto& active_sub_state){
      return active_sub_state.handle_event(e); 
    };
    auto reacted = std::visit(do_handle_event, active_sub_state_);
    return reacted | this->StateWrapper<State_>::handle_event(e);
  }

private:  
  StateMachine& state_machine_;
  to_variant_t<SubStateWrappers> active_sub_state_;
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
  : active_state_configuration_{std::get<StateMixin<TopState_>>(all_states_), *this}
  {
    init(std::index_sequence_for<States>());
  }

  // TODO copy, move ctor

  template <typename Event_>
  bool dispatch(const Event_& event) {
    bool reacted = active_state_configuration_.handle_event(event);
  

    return reacted;
  }

  template <typename State_>
  auto& get_state() {
    return std::get<StateMixin<State_>>(all_states_);
  }

  template <typename State_>
  auto& get_target_state_combination() {
    return std::get<state_id_v<State_>>(target_state_combinations_);
  }

private:
  template <auto ... I>
  void init(std::index_sequence<I...>) {
    ((std::get<I>(all_states_).target_state_combination_ = &std::get<I>(target_state_combinations_)), ...);
  }

  tuple_apply_t<StateMixin, States> all_states_;
  wrapper_t<TopState_> active_state_configuration_;
  std::array<state_combination_t<TopState_>, N> target_state_combinations_;
};


}
