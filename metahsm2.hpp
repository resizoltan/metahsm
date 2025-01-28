#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>
#include <optional>

#include "type_traits.hpp"
#include "trace.hpp"

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
    auto& current_target_branch = *static_cast<state_combination_t<TopState_>*>(target_state_combination_branch_p_);
    auto new_target_branch = state_combination_v<tuple_join_t<State_, super_state_recursive_t<State_>>>;
    if(merge_if_valid<TopState>(current_target_branch, new_target_branch)) {
      *static_cast<state_combination_t<TopState_>*>(target_state_combination_p_) |= state_combination_v<State_>;
    }
  }

  template <typename State_>
  State_& context() {
    return state_machine_->template get_state<State_>();
  }

public:
  template <typename Event_>
  bool react(Event_ const&) const { return false; }
  void on_entry() {}
  void on_exit() {}

protected:
  StateMachine<TopState_> * state_machine_;
  void * target_state_combination_branch_p_;
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
  using State<TopState>::react;
  using State_::on_entry;
  using State_::on_exit;

  StateMixin() {
    this->target_state_combination_branch_p_ = &target_state_combination_branch_;
    this->target_state_combination_p_ = &target_state_combination_;
  }

  void init(StateMachine<TopState> * state_machine) {
    this->state_machine_ = state_machine;
  }

  auto& target() {
    return target_state_combination_branch_;
  }

  auto& requested() {
    return target_state_combination_;
  }

private:
  state_combination_t<TopState> target_state_combination_branch_;
  state_combination_t<TopState> target_state_combination_; // for cleaner logging
};

template <typename State_>
struct WrapperArgs
{
  StateMixin<State_> & state;
  StateMachine<typename State_::TopState> & state_machine;
  state_combination_t<typename State_::TopState> const& target;
};

template <typename State_>
class StateWrapper {
public:
  using TopState = typename State_::TopState;
  using StateMachine = metahsm::StateMachine<TopState>;

  StateWrapper(StateMixin<State_>& state)
  : state_{state}
  {
    trace_enter<State_>();
    state_.on_entry();
  }

  ~StateWrapper()
  {
    trace_exit<State_>();
    state_.on_exit();
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    bool result = false;
    if constexpr(std::is_void_v<decltype(this->state_.react(e))>) {
      this->state_.react(e);
      result = true;
    }
    else {
      result = this->state_.react(e);
    }
    trace_react<State_>(result, state_.requested());
    return result; 
  }

  state_combination_t<TopState> active_state_combination_;

protected:
  StateMixin<State_> & state_;
};

template <typename State_>
class SimpleStateWrapper : public StateWrapper<State_>
{
public:
  using typename StateWrapper<State_>::StateMachine;
  using TopState = typename State_::TopState;

  SimpleStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args.state)
  { 
    this->active_state_combination_ = state_combination_v<State_>;
  }

  void exit(state_combination_t<TopState> const&) {}
  void enter(state_combination_t<TopState> const&) {}
};

template <typename State_>
class CompositeStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = typename State_::TopState;
  using SubStates = typename State_::SubStates;
  using SubStateWrappers = tuple_apply_t<wrapper_t, SubStates>;
  using typename StateWrapper<State_>::StateMachine;

  CompositeStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args.state),
    state_machine_{args.state_machine}
  {
    if(!change_state(args.target, type_identity<SubStates>{})) {
      using SubState = initial_state_t<State_>;
      active_sub_state_.template emplace<wrapper_t<SubState>>(
        WrapperArgs<SubState>{state_machine_.template get_state<SubState>(), state_machine_, args.target});
      update_state_combination();
    }
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    auto do_handle_event = overload{
      [&](auto& active_sub_state){ return active_sub_state.handle_event(e); },
      [](std::monostate) { return false; }
    };
    bool reacted = std::visit(do_handle_event, active_sub_state_);
    return reacted || this->StateWrapper<State_>::handle_event(e);
  }

  void exit(state_combination_t<TopState> const& target) {
    if ((target & state_combination_recursive_v<State_>).any()
        && (target & this->active_state_combination_ & ~state_combination_v<State_>).none()) {
      active_sub_state_ = std::monostate{};
    }
    else {
      auto do_execute_transition = overload{
        [&](auto& active_sub_state){ active_sub_state.exit(target); },
        [](std::monostate) { }
      };
      std::visit(do_execute_transition, active_sub_state_);
    }
  }

  void enter(state_combination_t<TopState> const& target) {
    auto do_execute_transition = overload{
      [&](auto& active_sub_state){ active_sub_state.enter(target); },
      [&](std::monostate) { this->change_state(target, type_identity<SubStates>{}); }
    };
    std::visit(do_execute_transition, active_sub_state_);
  }

private:  
  StateMachine& state_machine_;
  to_variant_t<tuple_join_t<std::monostate, SubStateWrappers>> active_sub_state_;

  template <typename ... SubState>
  bool change_state(state_combination_t<TopState> const& target, type_identity<std::tuple<SubState...>>) {
    bool changed = ((
      (state_combination_recursive_v<SubState> & target).any() ? 
        (
          active_sub_state_.template emplace<wrapper_t<SubState>>(WrapperArgs<SubState>{state_machine_.template get_state<SubState>(), state_machine_, target}),
          true
        ) : false
    ) || ...);
    if(changed) {
      update_state_combination();
    }
    return changed;
  }

  void update_state_combination() {
    auto updater = overload{
      [&](auto& substate){ return substate.active_state_combination_; },
      [](std::monostate){ return state_combination_t<TopState>{}; }
    };
    this->active_state_combination_ = state_combination_v<State_> | std::visit(updater, active_sub_state_);
  }

};

template <typename State_>
class OrthogonalStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = typename State_::TopState;
  using Regions = typename State_::Regions;
  // optional needed to control the order of constuction/destruction of tuple elements
  using RegionWrappers = tuple_apply_t<std::optional, tuple_apply_t<wrapper_t, Regions>>;
  using typename StateWrapper<State_>::StateMachine;

  OrthogonalStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args.state),
    state_machine_{args.state_machine}
  {
    init(args, type_identity<Regions>{});
  }

  ~OrthogonalStateWrapper() {
    auto deinit = [&](auto& ... region){
      (region.reset(), ...);
    };
    std::apply(deinit, regions_);
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    auto do_handle_event = [&](auto& ... region){
      bool reacted = false;
      ((reacted = region->handle_event(e) || reacted), ...);
      return reacted;
    };
    bool reacted = std::apply(do_handle_event, regions_);
    return reacted || this->StateWrapper<State_>::handle_event(e);
  }

  void exit(state_combination_t<TopState> const& target) {
    auto do_exit = [&](auto& ... region){
      (region->exit(target), ...);
    };
    std::apply(do_exit, regions_);
  }

  void enter(state_combination_t<TopState> const& target) {
    auto do_enter = [&](auto& ... region){
      (region->enter(target), ...);
    };
    std::apply(do_enter, regions_);
  }

private:
  template <typename ... Region>
  void init(WrapperArgs<State_> args, type_identity<std::tuple<Region...>>) {
    auto step1 = [&](auto& ... region){
      (region.emplace(WrapperArgs<Region>{state_machine_.template get_state<Region>(), state_machine_, args.target}), ...);
    };
    std::apply(step1, regions_);
    
    auto step2 = [&](auto& ... region){
      this->active_state_combination_ = (region->active_state_combination_ | ...);
    };
    std::apply(step2, regions_);
  }

  StateMachine& state_machine_;
  RegionWrappers regions_;
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
  : active_state_configuration_{WrapperArgs<TopState_>{get_state<TopState_>(), *this, {}}}
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
    active_state_configuration_.exit(target);
    active_state_configuration_.enter(target);
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
