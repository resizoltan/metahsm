// Copyright [2025] [Zoltán Rési]

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

template <typename Callable_, typename Variant_, typename TI_, auto ... I>
decltype(auto) visit(Callable_ && fun, Variant_ && v, std::integer_sequence<TI_, I...> const&) {
    using res_t = std::invoke_result_t<Callable_, decltype(*std::get_if<0>(&v))>;
    if constexpr(std::is_void_v<res_t>){
        const int i = v.index();
        ([&]{
            if(i==I) {
                fun(*std::get_if<I>(&v));
                return true;
            }
            return false;

        }()||... );
    }    
    else {
        const int i = v.index();
        std::invoke_result_t<Callable_, decltype(*std::get_if<0>(&v))> res;
        ([&]{
            if(i==I) {
                res = fun(*std::get_if<I>(&v));
                return true;
            }
            return false;

        }()||... )
        // should not reach here, but gcc complains
        || (res = fun(*std::get_if<0>(&v)), true);
        return res;
    }
}

template <typename Callable_, typename Variant_>
decltype(auto) visit(Callable_ && fun, Variant_ && v) {
    return visit(std::forward<Callable_&&>(fun), std::forward<Variant_&&>(v), 
        std::make_index_sequence<std::variant_size_v<std::remove_reference_t<Variant_>>>());
}

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

struct NOT_IMPLEMENTED
{};

class StateImplBase
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
  NOT_IMPLEMENTED react(Event_ const&);
  NOT_IMPLEMENTED on_entry();
  NOT_IMPLEMENTED on_exit();

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
  template <typename Event_>
  NOT_IMPLEMENTED react(Event_ const&);
  using State_::on_entry;
  using State_::on_exit;
  sc_t last{0};
  sc_t last_recursive{0};
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
  using State = State_;
  using Mixin = StateMixin<State>;
  template <typename Event_>
  static constexpr bool has_react = !std::is_same_v<NOT_IMPLEMENTED, decltype(std::declval<Mixin>().react(std::declval<Event_>()))>;

  StateWrapper(WrapperArgs<State_> args)
  : state_{args.state},
    state_machine_{args.state_machine}
  {
    trace_enter<State_>();
    if constexpr(!std::is_same_v<NOT_IMPLEMENTED, decltype(state_.on_entry())>) {
      state_.on_entry();
    }
  }

  ~StateWrapper()
  {
    trace_exit<State_>();
    if constexpr(!std::is_same_v<NOT_IMPLEMENTED, decltype(state_.on_entry())>) {
      state_.on_exit();
    }
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    bool result = false;
    if constexpr(std::is_void_v<decltype(state_.react(e))>) {
      state_.react(e);
      result = true;
    }
    else {
      result = state_.react(e);
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
  template <typename Event_>
  static constexpr bool HAS_REACT_RECURSIVE = StateWrapper<State_>::template has_react<Event_>;

  SimpleStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args)
  { 
    this->state().last_recursive = state_combination_v<State_>;
  }

  void exit(state_combination_t<TopState> const&) {}
  void enter(state_combination_t<TopState> const&) {}
};

template <typename Event_, typename StateWrappers_>
struct has_react;

template <typename Event_, typename ... StateWrapper_>
struct has_react<Event_, std::tuple<StateWrapper_...>>
{
  static constexpr bool value = (StateWrapper_::template HAS_REACT_RECURSIVE<Event_> || ...);
};


template <typename State_>
class CompositeStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = top_state_t<State_>;
  using SubStates = typename State_::SubStates;
  using SubStateWrappers = tuple_apply_t<wrapper_t, SubStates>;
  using typename StateWrapper<State_>::StateMachine;
  static constexpr std::size_t N = std::tuple_size_v<SubStates>;
  template <typename Event_> // TODO check if needed
  static constexpr bool HAS_REACT_RECURSIVE = StateWrapper<State_>::template has_react<Event_> | has_react<Event_, SubStateWrappers>::value;

  CompositeStateWrapper(WrapperArgs<State_> args)
  : StateWrapper<State_>(args)
  {
    if (args.target & state_combination_v<SubStates>) {
      next_state_id_ = bit_index(args.target & state_combination_v<SubStates>);
    }
    else {
      next_state_id_ = state_id_v<initial_state_t<State_>>;
    }
    enter(args.target);
  }

  template <typename Event_>
  bool handle_event(const Event_& e) {
    auto do_handle_event = overload{
      [&](auto& active_sub_state){
        if constexpr(std::remove_reference_t<decltype(active_sub_state)>::template HAS_REACT_RECURSIVE<Event_>) {
          return active_sub_state.handle_event(e); 
        }
        return false;
      },
      [](std::monostate) { return false; }
    };
    bool reacted = visit(do_handle_event, active_sub_state_);
    if constexpr(StateWrapper<State_>::template has_react<Event_>) {
      return reacted || this->StateWrapper<State_>::handle_event(e);
    }
    else {
      return reacted;
    }
  }

  void exit(state_combination_t<TopState> const& target) {
    if ((target & state_combination_recursive_v<State_>)) {
      if ((target & this->state().last_recursive & ~state_combination_v<State_>)) {
        auto sub_exit = overload{
            [&](auto& sub) { sub.exit(target); },
            [](std::monostate) { }
        };
        visit(sub_exit, active_sub_state_);
      }
      else {
        active_sub_state_ = std::monostate{};
        if (target & state_combination_v<SubStates>) {
          next_state_id_ = bit_index(target & state_combination_v<SubStates>);
        }
        else {
          next_state_id_ = state_id_v<initial_state_t<State_>>;
        }
      }
    }
  }

template <auto ... I>
  void enter(state_combination_t<TopState> const& target, std::index_sequence<I...> const&) {
    if(next_state_id_) {
      const int i = next_state_id_ - state_id_v<std::tuple_element_t<0, SubStates>>;
        ([&]{
            if(i==I) {
                using SubState = std::tuple_element_t<I, SubStates>;
                auto& sub_state = this->state_machine_.template get_state<SubState>();
                active_sub_state_.template emplace<wrapper_t<SubState>>(WrapperArgs<SubState>{sub_state, this->state_machine_, target});
                this->state().last_recursive = state_combination_v<State_> | sub_state.last_recursive;
                this->state().last = state_combination_v<SubState>;
                next_state_id_ = 0;
                return true;
            }
            return false;

        }()||... );
    }
    else {
      auto sub_enter = overload{
        [&](auto& sub) { sub.enter(target); },
        [](std::monostate) { }
      };
      visit(sub_enter, active_sub_state_);
    }
  }

  void enter(state_combination_t<TopState> const& target) {
    enter(target, std::make_index_sequence<N>());
  }

private:  
  to_variant_t<tuple_join_t<std::monostate, SubStateWrappers>> active_sub_state_;
  std::size_t next_state_id_;
};

template <typename State_>
class OrthogonalStateWrapper : public StateWrapper<State_>
{
public:
  using TopState = top_state_t<State_>;
  using Regions = typename State_::Regions;
  // optional needed to control the order of constuction/destruction of tuple elements
  using RegionWrappers = tuple_apply_t<wrapper_t, Regions>;
  using RegionWrapperOptionals = tuple_apply_t<std::optional, RegionWrappers>;
  using typename StateWrapper<State_>::StateMachine;
  template <typename Event_>
  static constexpr bool HAS_REACT_RECURSIVE = StateWrapper<State_>::template has_react<Event_> | has_react<Event_, RegionWrappers>::value;

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
    if constexpr(StateWrapper<State_>::template has_react<Event_>) {
      return reacted || this->StateWrapper<State_>::handle_event(e);
    }
    else {
      return reacted;
    }
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

  RegionWrapperOptionals regions_;
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
      if constexpr(std::is_base_of_v<DeepHistoryBase, Target_>) {
        return transition<TargetState_>(get_state<TargetState_>().last_recursive);
      }
      else {
        return  transition<TargetState_>(get_state<TargetState_>().last);
      }
    }
    else {       
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
