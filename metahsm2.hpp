#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>
#include <optional>
#include <bitset>

#include "type_traits.hpp"

namespace metahsm {

//=====================================================================================================//
//                                     STATE TEMPLATE - USER API                                       //
//=====================================================================================================//

template <typename TopState_>
class StateMachine;

struct Result
{
  bool reacted;
  std::size_t target_state_index;
};

namespace detail {
template <typename TopState_>
class State_ : public StateBase
{
public:
  using State = State_<TopState_>;
  using TopState = TopState_;

  template <typename TargetState_>
  auto transition() {
      //(static_assert(is_in_context_recursive_v<_TargetStateDef, TopStateDef>),...);
    target_state_index_ = index_v<TargetState_, all_states_t<TopState>>;
  }

protected:
  std::size_t target_state_index_;
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

  StateMixin(int i){}

  template <typename Event_>
  Result handle_event(const Event_& e) {
    if constexpr(has_reaction_to_event_v<State_, Event_>) {
      Result result{this->react(e), this->target_state_index_};
      this->target_state_index_ = 0;
      return result;
    }
    return {false, 0};
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

protected:
  StateMixin<State_>& state_;
};

template <typename State_>
class SimpleStateWrapper : public StateWrapper<State_>
{
public:
  using typename StateWrapper<State_>::StateMachine;

  SimpleStateWrapper(StateMixin<State_>& state, StateMachine&)
  : StateWrapper<State_>(state)
  { }

  template <typename Event_>
  auto handle_event(const Event_& e) {
    return this->state_.handle_event(e);
  }
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
      state_machine.template get<initial_state_t<State_>>(), state_machine_}
  { }

  template <typename Event_>
  auto handle_event(const Event_& e) {
    auto do_handle_event = [&](auto& active_sub_state){
      return active_sub_state.handle_event(e); 
    };
    auto reacted = std::visit(do_handle_event, active_sub_state_);
    return reacted | this->state_.handle_event(e);
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
  : StateMachine(std::index_sequence<std::tuple_size_v<States>>())
  {}

  template <typename Event_>
  bool dispatch(const Event_& event) {
    bool reacted = active_state_configuration_.handle_event(event);
    return false;
  }

  template <typename State_>
  auto& get() {
    return std::get<StateMixin<State_>>(all_states_);
  }

private:
  template <auto ... I>
  StateMachine(std::index_sequence<I...>)
  : all_states_{1, 1, 1, 1, 1},
    active_state_configuration_{std::get<StateMixin<TopState_>>(all_states_), *this}
  {}

  tuple_apply_t<StateMixin, States> all_states_;
  wrapper_t<TopState_> active_state_configuration_;
  std::bitset<N> requested_transition_;
};


}
