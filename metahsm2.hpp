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
class StateImpl_ : public StateBase
{
public:
  using State = StateImpl_<TopState_>;
  using TopState = TopState_;

protected:

  template <typename State_>
  void transition() {
    auto& current_target_branch = *static_cast<state_combination_t<TopState_>*>(target_state_combination_p_);
    auto new_target_branch = state_combination_v<tuple_join_t<State_, super_state_recursive_t<State_>>>;
    merge_if_valid<TopState>(current_target_branch, new_target_branch);
  }

  template <typename State_>
  State_& context() {
    return state_machine_->template get_state<State_>();
  }

  void react();
  void on_entry();
  void on_exit();

protected:
  StateMachine<TopState_> * state_machine_;
  void * target_state_combination_p_;
};
}

template <typename TopState_>
using State = detail::StateImpl_<TopState_>;

template <typename State_>
class StateMixin : public State_
{
public:
  using typename State_::TopState;
  using State_::react;
  using State_::on_entry;
  using State_::on_exit;

  StateMixin() {
    this->target_state_combination_p_ = &target_state_combination_;
  }

  void init(StateMachine<TopState> * state_machine) {
    this->state_machine_ = state_machine;
  }

  template <typename Event_>
  bool react(Event_ const&) {
    return false;
  }

  void on_entry() {}
  void on_exit() {}

  auto& target() {
    return target_state_combination_;
  }

private:
  state_combination_t<TopState> target_state_combination_;
};


template <typename State_>
class StateWrapper {
public:
  using StateMachine = metahsm::StateMachine<typename State_::TopState>;

  StateWrapper(StateMixin<State_>& state)
  : state_{state}
  {
    state_.on_entry();
  }

  ~StateWrapper()
  {
    state_.on_exit();
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    if constexpr(std::is_void_v<decltype(this->state_.react(e))>) {
      this->state_.react(e);
      return true;
    }
    else {
      return this->state_.react(e);
    }
    return false; 
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
  using TopState = typename State_::TopState;
  using SubStates = typename State_::SubStates;
  using SubStateWrappers = tuple_apply_t<wrapper_t, SubStates>;
  using typename StateWrapper<State_>::StateMachine;

  CompositeStateWrapper(StateMixin<State_>& state, StateMachine& state_machine)
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
  using StateMixins = tuple_apply_t<StateMixin, States>;
  static constexpr std::size_t N = std::tuple_size_v<States>;

  StateMachine()
  : active_state_configuration_{get_state<TopState_>(), *this}
  {
    auto init = [&](auto& ... state) {
      (state.init(this), ...);
    };
    std::apply(init, all_states_);
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
    return std::get<StateMixin<State_>>(all_states_);
  }

  auto compute_target_state_combination() {
    auto find = [&](auto& ... state) {
      state_combination_t<TopState_> target{};
      (merge_if_valid<TopState_>(target, state.target()), ...);
      ((state.target() = {}), ...);
      return target;
    };
    return std::apply(find, all_states_);
  }

private:
  StateMixins all_states_;
  wrapper_t<TopState_> active_state_configuration_;
};


}
