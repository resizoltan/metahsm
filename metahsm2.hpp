#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>
#include <optional>
#include <random>
#include <cmath>

#include "type_traits.hpp"
#include "trace.hpp"

namespace metahsm {

//=====================================================================================================//
//                                     STATE TEMPLATE - USER API                                       //
//=====================================================================================================//

class StateMachineBase
{
public:
  template <typename Callable_>
  void transition_action(Callable_ const& action) {
    action_ = action;
  }

protected:
  std::optional<std::function<void()>> action_;
};
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

class StateImplBase : public StateBase
{
public:
  // internal
  StateMachineBase & state_machine_;

  template <typename Target_>
  bool transition() {
    if constexpr(std::is_base_of_v<HistoryBase, Target_>) {
      return state_machine<top_state_t<typename Target_::State>>().template transition<Target_>();
    }
    else {
      return state_machine<top_state_t<Target_>>().template transition<Target_>();
    }
  }

  template <typename Callable_>
  bool transition_action(Callable_ const& action)  {
    state_machine_.transition_action(action);
    return true;
  }

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
    return state_machine<top_state_t<State_>>().template get_state<State_>();
  }

  template <typename State_>
  bool is_in_state() {
    return state_machine<top_state_t<State_>>().template is_in_state<State_>();
  }

  template <typename Event_>
  bool react(Event_ const&) const { return false; }
  void on_entry() {}
  void on_exit() {}

private:
  template <typename TopState>
  auto& state_machine() {
    return static_cast<StateMachine<TopState>&>(state_machine_); // TODO rtti check
  }
};

template <typename TopState_>
struct StateImpl : public StateImplBase
{
  using State = StateImpl<TopState_>;
  using Region = State;
  using TopState = TopState_;
  using Conf = void;
};

template <template <typename> typename TopStateTemplate_>
struct StateTemplateImpl : public StateImplBase
{
  using State = StateTemplateImpl<TopStateTemplate_>;
  using Region = State;
  template <typename Config_>
  using TopStateTemplate = TopStateTemplate_<Config_>;
};

template <typename Config_>
struct Config
{
    using Conf = Config_;
    using TopState = void;
};

template <typename TopState_>
struct TopStateRebind
{
  using Conf = TopStateRebind<TopState_>;
  using TopState = TopState_;
};

template <typename State_>
struct StateMixin : public State_
{
public:
  using TopState = top_state_t<State_>;
  using sc_t = state_combination_t<TopState>;
  using State_::react;
  using StateImplBase::react;
  using State_::on_entry;
  using State_::on_exit;
  sc_t last;
  sc_t last_recursive;
};

template <typename TopState_>
using State = StateImpl<TopState_>;

template <typename TopState_>
using Region = State<TopState_>;

template <template <typename> typename TopStateTemplate_>
using StateTemplate = StateTemplateImpl<TopStateTemplate_>;

template <template <typename> typename TopStateTemplate_>
using RegionTemplate = StateTemplateImpl<TopStateTemplate_>;


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
  using TopState = top_state_t<State_>;
  using StateMachine = metahsm::StateMachine<TopState>;

  StateWrapper(WrapperArgs<State_> args)
  : state_{args.state},
    state_machine_{args.state_machine}
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
    state_machine_.template post_react<State_>(result);
    return result; 
  }

  auto& state() {
    return state_;
  }

protected:
  StateMixin<State_> & state_;
  StateMachine & state_machine_;
};

template <typename State_>
class SimpleStateWrapper : public StateWrapper<State_>
{
public:
  using typename StateWrapper<State_>::StateMachine;
  using TopState = top_state_t<State_>;

  SimpleStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args)
  { 
    this->state().last_recursive = state_combination_v<State_>;
  }

  void exit(state_combination_t<TopState> const&) {}
  void enter(state_combination_t<TopState> const&) {}
};

template <typename State_>
class CompositeStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = top_state_t<State_>;
  using SubStates = typename State_::SubStates;
  using SubStateWrappers = tuple_apply_t<wrapper_t, SubStates>;
  using typename StateWrapper<State_>::StateMachine;
  using LookupTable = std::array<void(CompositeStateWrapper<State_>::*)(state_combination_t<TopState> const&), std::tuple_size_v<SubStates>>;

  CompositeStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args)
  {
    enter(args.target);
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
    if ((target & state_combination_recursive_v<State_>)) {
      if ((target & this->state().last_recursive & ~state_combination_v<State_>)) {
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
      [&](std::monostate) {
        std::size_t state_id_to_enter;
        if (target & state_combination_v<SubStates>) {
          state_id_to_enter = bit_index(target & state_combination_v<SubStates>);
        }
        else {
          state_id_to_enter = state_id_v<initial_state_t<State_>>;
        }
        std::size_t local_id = state_id_to_enter - state_id_v<std::tuple_element_t<0, SubStates>>;
        std::invoke(lookup_table_[local_id], this, target);
      }
    };
    std::visit(do_execute_transition, active_sub_state_);
  }

private:  
  to_variant_t<tuple_join_t<std::monostate, SubStateWrappers>> active_sub_state_;

  template <typename ... SubState_>
  static constexpr LookupTable make_lookup_table(type_identity<std::tuple<SubState_ ...>>) {
    return {&CompositeStateWrapper<State_>::change_state<SubState_>...};
  }
  static constexpr LookupTable lookup_table_ = make_lookup_table(type_identity<SubStates>{});

  template <typename SubState_>
  void change_state(state_combination_t<TopState> const& target) {
    auto& sub_state = this->state_machine_.template get_state<SubState_>();
    active_sub_state_.template emplace<wrapper_t<SubState_>>(WrapperArgs<SubState_>{sub_state, this->state_machine_, target});
    this->state().last_recursive = state_combination_v<State_> | sub_state.last_recursive;
    this->state().last = state_combination_v<SubState_>;
  }

};

template <typename State_>
class OrthogonalStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = top_state_t<State_>;
  using Regions = typename State_::Regions;
  // optional needed to control the order of constuction/destruction of tuple elements
  using RegionWrappers = tuple_apply_t<std::optional, tuple_apply_t<wrapper_t, Regions>>;
  using typename StateWrapper<State_>::StateMachine;

  OrthogonalStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args)
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
      this->state().last_recursive = state_combination_v<State_> | (region->state().last_recursive | ...);
      this->state().last = state_combination_v<State_> | (region->state().last | ...);
    };
    std::apply(step2, regions_);
  }

private:
  template <typename ... Region>
  void init(WrapperArgs<State_> args, type_identity<std::tuple<Region...>>) {
    auto step1 = [&](auto& ... region){
      (region.emplace(WrapperArgs<Region>{this->state_machine_.template get_state<Region>(), this->state_machine_, args.target}), ...);
    };
    std::apply(step1, regions_);
    
    auto step2 = [&](auto& ... region){
      this->state().last_recursive = state_combination_v<State_> | (region->state().last_recursive | ...);
      this->state().last = state_combination_v<State_> | (region->state().last | ...);
    };
    std::apply(step2, regions_);
  }

  RegionWrappers regions_;
};


//=====================================================================================================//
//                                         STATE MACHINE                                               //
//=====================================================================================================//

template <typename Mixin_>
struct MixinHolder
{
  using StateMachine = StateMachine<typename Mixin_::TopState>;

  MixinHolder(StateMachine & state_machine)
  : mixin{{{{.state_machine_ = state_machine}}}}
  {}

  Mixin_ mixin;
};

template <typename TopState_>
class StateMachine : public StateMachineBase
{
public:
  using States = all_states_t<TopState_>;
  using StateMixins = tuple_apply_t<MixinHolder, tuple_apply_t<StateMixin, States>>;
  using sc_t = state_combination_t<TopState_>;
  static constexpr std::size_t N = std::tuple_size_v<States>;

  StateMachine()
  : all_states_{init_states(type_identity<States>{})},
    active_state_configuration_{WrapperArgs<TopState_>{get_state<TopState_>(), *this, {}}},
    target_branch_{0},
    target_{0}
  { }

  // TODO copy, move ctor

  template <typename Event_>
  bool dispatch(const Event_& event = {}) {
    trace_event<Event_>();
    bool reacted = active_state_configuration_.handle_event(event);
    active_state_configuration_.exit(target_branch_);
    execute_actions();
    active_state_configuration_.enter(target_branch_);
    target_ = 0;
    target_branch_ = 0;
    return reacted;
  }

  template <typename State_>
  auto& get_state() {
    return std::get<MixinHolder<StateMixin<State_>>>(all_states_).mixin;
  }

  template <typename State_>
  bool is_in_state() {
    return (active_state_configuration_.state().last_recursive & state_combination_v<State_>);
  }

  template <typename State_>
  void post_react(bool result) {
    trace_react<State_>(result, target_);
    target_ = 0;
  }

private:
  StateMixins all_states_;
  wrapper_t<TopState_> active_state_configuration_;
  sc_t target_branch_;
  sc_t target_;

  friend class StateImplBase;

  template <typename>
  auto& sm() {
    return *this;
  }

  template <typename ... State_>
  auto init_states(type_identity<std::tuple<State_...>>) {
    return StateMixins{sm<State_>()...};
  }

  template <typename Target_>
  bool transition() {
    if constexpr(std::is_base_of_v<HistoryBase, Target_>) {
      using TargetState_ = typename Target_::State;
      using TopState = top_state_t<TargetState_>;
      if constexpr(std::is_base_of_v<DeepHistoryBase, Target_>) {
        return transition<TargetState_>(get_state<TargetState_>().last_recursive);
      }
      else {
        return  transition<TargetState_>(get_state<TargetState_>().last);
      }
    }
    else {       
      using TopState = top_state_t<Target_>;
      return transition<Target_>(sc_t{});
    }
  }

  template <typename TargetState_>
  bool transition(sc_t const& history) {
    sc_t new_target = state_combination_v<TargetState_>;
    sc_t new_target_branch = new_target | history | state_combination_v<super_state_recursive_t<TargetState_>>;
    bool valid = is_valid<TopState_>(target_branch_, new_target_branch);
    if(valid) {
      target_branch_ |= new_target_branch;
      target_ |= new_target;
    }
    return valid;
  }

  void execute_actions() {
    if(this->action_) {
      std::invoke(*this->action_);
      this->action_.reset();
    }
  }
};


}
