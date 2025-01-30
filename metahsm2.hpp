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
class StateImplBase;

struct HistoryBase
{};

struct DeepHistoryBase : HistoryBase
{};

template <typename State_>
struct History : HistoryBase
{
  using State = State_;
  struct Shallow : HistoryBase
  {
    using State = State_;
  };

  struct Deep : DeepHistoryBase
  {
    using State = State_;
  };
};

struct StateMixinBase;
template <typename TopState_>
struct StateMixinCommon;

class StateImplBase : StateBase
{
public:
  template <typename Target_>
  bool transition() {
    if constexpr(std::is_base_of_v<HistoryBase, Target_>) {
      using TargetState_ = typename Target_::State;
      using TopState = top_state_t<TargetState_>;
      if constexpr(std::is_base_of_v<DeepHistoryBase, Target_>) {
        return mixin<TopState>().template transition_deep<TargetState_>();
      }
      else {
        return mixin<TopState>().template transition_shallow<TargetState_>();
      }
    }
    else {       
      using TopState = top_state_t<Target_>;
      return mixin<TopState>().template transition<Target_>();
    }
  }

  template <typename Callable_>
  bool transition_action(Callable_ const& action);

  template <typename SourceState_>
  bool transition_action(void(SourceState_::*action)()) {
    if(&context<SourceState_>() != this) {
      return false;
    }
    auto state = static_cast<SourceState_*>(this);
    return transition_action(std::bind(action, state));
  }

  template <typename State_>
  State_& context() {
    using TopState = top_state_t<State_>;
    return mixin<TopState>().state_machine->template get_state<State_>();
  }

  template <typename State_>
  bool is_in_state() {
    using TopState = top_state_t<State_>;
    return mixin<TopState>().state_machine->template is_in_state<State_>();
  }

  template <typename Event_>
  bool react(Event_ const&) const { return false; }
  void on_entry() {}
  void on_exit() {}

private:
  template <typename TopState>
  auto& mixin() {
    return static_cast<StateMixinCommon<TopState>&>(*mixin_);
  }

  friend class StateMixinBase;
  StateMixinBase * mixin_;
};

template <typename TopState_>
struct StateImpl : public StateImplBase
{
  using State = StateImpl<TopState_>;
  using Region = State;
  using TopState = TopState_;
  using Conf = void;
};

struct StateMixinBase
{
  void init(StateImplBase * state) {
    state->mixin_ = this;
  }
  std::optional<std::function<void()>> action;
};

template <typename Callable_>
bool StateImplBase::transition_action(Callable_ const& action) {
  mixin_->action = action;
  return true;
}

template <typename TopState_>
struct StateMixinCommon : StateMixinBase
{
  using sc_t = state_combination_t<TopState_>;
  StateMachine<TopState_> * state_machine;
  sc_t target_branch;
  sc_t target; // for cleaner logging
  sc_t last;
  sc_t last_recursive;

  template <typename State_>
  void init(State_ * state, StateMachine<TopState_> * state_machine) {
    StateMixinBase::init(state);
    this->state_machine = state_machine;
  }

  template <typename TargetState_>
  bool transition(sc_t const& history = {}) {
    sc_t new_target = state_combination_v<TargetState_>;
    sc_t new_target_branch = new_target | history | state_combination_v<super_state_recursive_t<TargetState_>>;
    bool valid = merge_if_valid<TopState_>(target_branch, new_target_branch);
    if(valid) {
      target |= new_target;
    }
    return valid;
  }

  template <typename TargetState_>
  bool transition_shallow() {
    return transition<TargetState_>(state_machine->template get_state<TargetState_>().common.last);
  }

  template <typename TargetState_>
  bool transition_deep() {
    return transition<TargetState_>(state_machine->template get_state<TargetState_>().common.last_recursive);
  }

};



template <typename TopState_>
using State = StateImpl<TopState_>;

template <typename TopState_>
using Region = State<TopState_>;

template <typename State_>
struct StateMixin : public State_
{
  using typename State_::TopState;
  using State_::react;
  using State<TopState>::react;
  using State_::on_entry;
  using State_::on_exit;

  void init(StateMachine<TopState> * state_machine) {
    common.init(this, state_machine);
  }

  StateMixinCommon<top_state_t<State_>> common;
};

template <typename State_>
struct WrapperArgs
{
  StateMixin<State_> & state;
  StateMachine<top_state_t<State_>> & state_machine;
  state_combination_t<top_state_t<State_>> const& target;
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
    trace_react<State_>(result, state().common.target);
    return result; 
  }

  auto& state() {
    return state_;
  }

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
    this->state().common.last_recursive = state_combination_v<State_>;
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
      change_state<initial_state_t<State_>>(args.target);
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
    if ((target & state_combination_recursive_v<State_>).any()) {
      if ((target & this->state().common.last_recursive & ~state_combination_v<State_>).any()) {
        auto do_execute_transition = overload{
          [&](auto& active_sub_state){ active_sub_state.exit(target); },
          [](std::monostate) { }
        };
        std::visit(do_execute_transition, active_sub_state_);
      }
      else {
        active_sub_state_ = std::monostate{};
      }
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
          change_state<SubState>(target),
          true
        ) : false
    ) || ...);
    return changed;
  }

  template <typename SubState_>
  void change_state(state_combination_t<TopState> const& target) {
    auto& sub_state = state_machine_.template get_state<SubState_>();
    active_sub_state_.template emplace<wrapper_t<SubState_>>(WrapperArgs<SubState_>{sub_state, state_machine_, target});
    this->state().common.last_recursive = state_combination_v<State_> | sub_state.common.last_recursive;
    this->state().common.last = state_combination_v<SubState_>;
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

    auto step2 = [&](auto& ... region){
      this->state().common.last_recursive = state_combination_v<State_> | (region->state().common.last_recursive | ...);
      this->state().common.last = state_combination_v<State_> | (region->state().common.last | ...);
    };
    std::apply(step2, regions_);
  }

private:
  template <typename ... Region>
  void init(WrapperArgs<State_> args, type_identity<std::tuple<Region...>>) {
    auto step1 = [&](auto& ... region){
      (region.emplace(WrapperArgs<Region>{state_machine_.template get_state<Region>(), state_machine_, args.target}), ...);
    };
    std::apply(step1, regions_);
    
    auto step2 = [&](auto& ... region){
      this->state().common.last_recursive = state_combination_v<State_> | (region->state().common.last_recursive | ...);
      this->state().common.last = state_combination_v<State_> | (region->state().common.last | ...);
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
    trace_event<Event_>();
    bool reacted = active_state_configuration_.handle_event(event);
    auto target = compute_target_state_combination();
    active_state_configuration_.exit(target);
    execute_actions();
    active_state_configuration_.enter(target);
    return reacted;
  }

  template <typename State_>
  auto& get_state() {
    return std::get<StateMixin<State_>>(all_states_);
  }

  template <typename State_>
  bool is_in_state() {
    return (active_state_configuration_.state().common.last_recursive & state_combination_v<State_>).any();
  }

private:
  StateMixins all_states_;
  wrapper_t<TopState_> active_state_configuration_;
  
  auto compute_target_state_combination() {
    auto find = [&](auto& ... state) {
      state_combination_t<TopState_> target{};
      auto resolve_conflicts = [&](auto& s) {
        if(!merge_if_valid<TopState_>(target, s.common.target_branch)) {
          s.common.action.reset();
        }
      };
      (resolve_conflicts(state), ...);
      ((state.common.target_branch = {}), ...);
      return target;
    };
    return std::apply(find, all_states_);
  }

  void execute_actions() {
    auto exec_all = [&](auto &... state) {
      auto exec = [&](auto & s) {
        auto& action = s.common.action;
        if(action) {
          std::invoke(*action);
        }
        action.reset();
      };
      (exec(state), ...);
    };
    std::apply(exec_all, all_states_);
  }
};


}
