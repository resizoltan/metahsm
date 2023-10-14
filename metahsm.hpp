#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

#include "type_traits.hpp"

namespace metahsm {

//=====================================================================================================//
//                                     TYPE ERASED TRANSITION                                          //
//=====================================================================================================//

template <typename _TopStateDef>
using TransitionTrampoline = void(*)(mixin_t<_TopStateDef>&);

template <typename _TopStateDef, typename _StateDef>
void transition_trampoline(mixin_t<_TopStateDef>& top_state_mixin) {
    top_state_mixin.template executeTransition<_StateDef>();
}

template <typename _TopStateDef, typename ... _StateDef>
struct TransitionMerger;

template <typename _TopStateDef>
struct Inner
{
    virtual TransitionMerger<_TopStateDef> merge();
};

template <typename _TopStateDef, typename _StateDef>
struct TransitionMergerBase {
    TransitionMergerBase(Inner<_TopStateDef>& inner)
    : inner_{inner}
    {}
    Inner<_TopStateDef>& get_inner(state_spec<_StateDef>) { return inner_; }

    Inner<_TopStateDef>& inner_;
};

template <typename _TopStateDef, typename ... _StateDef>
struct TransitionMerger : TransitionMergerBase<_TopStateDef, _StateDef>...
{
    template <typename ... I>
    TransitionMerger(I... inner) 
    : TransitionMergerBase<_TopStateDef, _StateDef>(inner)...
    { }

    virtual TransitionMerger<_TopStateDef> merge(TransitionMerger<_TopStateDef> other) {
        return other;
    }

    template <typename _OtherStateDef>
    TransitionMerger<_TopStateDef> merge() { return this->get_inner(state_spec<_OtherStateDef>{}).merge(); }
};

template <typename _TopStateDef, typename _TargetStateDef, typename ... _StateDef>
struct RegularTransitionMerger;

template <typename _TopStateDef, typename _TargetStateDef, typename _OtherStateDef, typename ... _StateDef>
struct RegularInner : Inner<_TopStateDef>
{
    TransitionMerger<_TopStateDef> merge() override {
        return RegularTransitionMerger<_TopStateDef, std::tuple<_TargetStateDef, _OtherStateDef>, _StateDef...>{};
    }
};

template <typename _TopStateDef, typename _TargetStateDef, typename _OtherStateDef, typename ... _StateDef>
struct RegularTransitionMergerBase {
    RegularInner<_TopStateDef, _TargetStateDef, _OtherStateDef, _StateDef...>& get_regular_inner(state_spec<_OtherStateDef>) { return regular_inner_; }
    RegularInner<_TopStateDef, _TargetStateDef, _OtherStateDef, _StateDef...> regular_inner_;
};

template <typename _TopStateDef, typename _TargetStateDef, typename ... _StateDef>
struct RegularTransitionMerger : TransitionMerger<_TopStateDef, _StateDef...>, RegularTransitionMergerBase<_TopStateDef, _TargetStateDef, _StateDef, _StateDef...>...
{
    RegularTransitionMerger()
    : TransitionMerger<_TopStateDef, _StateDef...>(this->get_regular_inner(state_spec<_StateDef>{})...)
    {}

    TransitionMerger<_TopStateDef> merge(TransitionMerger<_TopStateDef> other) override {
        return other.template merge<_TargetStateDef>();
    }
};

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
        this->mixin().set_transition(&transition_trampoline<_TopStateDef, _TargetStateDef>);
    }

public:
    decltype(auto) mixin() {
        return static_cast<mixin_t<_StateDef>&>(*this);
    }

    static decltype(auto) top_state_spec()
    {
        return state_spec<TopStateDef>{};
    }
};

//=====================================================================================================//
//                    STATE MIXINS - INTERNAL, IMPLEMENT STATE MACHINE BEHAVIOR                        //
//=====================================================================================================//

template <typename _TopStateDef>
class StateMachine;

template <typename _StateDef>
class StateMixin : public _StateDef
{
public:
    using StateDef = _StateDef;
    using TopStateDef = typename decltype(_StateDef::top_state_spec())::type;
    using SuperStateDef = super_state_t<_StateDef>;
    using SuperStateMixin = mixin_t<SuperStateDef>;

    StateMixin(SuperStateMixin& super_state_mixin)
    : super_state_mixin_{super_state_mixin}
    {
        if constexpr (has_entry_action_v<_StateDef>) {
            this->on_entry();
        }
    }

    ~StateMixin() {
        if constexpr (has_exit_action_v<_StateDef>) {
            this->on_exit();
        }    
    }

    template <typename _Event>
    auto handleEvent(const _Event& e) {
        this->transition_ = nullptr;
        if constexpr(has_reaction_to_event_v<_StateDef, _Event>) {
            return std::make_tuple(this->react(e), this->transition_);
        }
        else {
            return std::make_tuple(false, nullptr);
        }
    }

    template <typename _ContextDef>
    _ContextDef& context() {
        if constexpr(std::is_same_v<_ContextDef, _StateDef>) {
            return *this;
        }
        else {
            return super_state_mixin_.template context<_ContextDef>();
        }
    }

    void set_transition(TransitionTrampoline<TopStateDef> transition) {
        transition_ = transition;
    }

protected:
    SuperStateMixin& super_state_mixin_;
    TransitionTrampoline<TopStateDef> transition_;
};


template <typename _StateDef>
class CompositeStateMixin : public StateMixin<_StateDef>
{
public:
    using SubStates = typename _StateDef::SubStates;
    using typename StateMixin<_StateDef>::TopStateDef;
    using typename StateMixin<_StateDef>::SuperStateMixin;

    template <typename _TargetStateSpec, typename _DirectSubStateToEnter>
    CompositeStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec target_spec, state_spec<_DirectSubStateToEnter> enter_spec)
    : StateMixin<_StateDef>(super_state_mixin),
      active_sub_state_{ 
        std::in_place_type<mixin_t<_DirectSubStateToEnter>>,
        this->mixin(),
        target_spec
      }
    {}

    template <typename _TargetStateSpec>
    CompositeStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec spec)
    : CompositeStateMixin(super_state_mixin, spec, state_spec<direct_substate_to_enter_t<_StateDef, typename _TargetStateSpec::type>>{})
    {}

    template <typename _Event>
    auto handleEvent(const _Event& e) {
        auto do_handle_event = [&](auto& active_sub_state){ return active_sub_state.handleEvent(e); };
        auto [substate_reacted, transition] = std::visit(do_handle_event, active_sub_state_);
        return substate_reacted ? std::make_tuple(substate_reacted, transition) : StateMixin<_StateDef>::template handleEvent<_Event>(e);
    }

    template <typename _TargetStateDef>
    void executeTransition() {
        auto is_target_in_context = [&](auto& active_sub_state) { 
            return is_in_context_recursive_v<_TargetStateDef, typename std::remove_reference_t<decltype(active_sub_state)>::StateDef>; 
        };

        if(std::visit(is_target_in_context, active_sub_state_)) {
            auto do_execute_transition = [](auto& active_sub_state){ active_sub_state.template executeTransition<_TargetStateDef>(); };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            using Initial = direct_substate_to_enter_t<_StateDef, _TargetStateDef>;
            active_sub_state_.template emplace<mixin_t<Initial>>(
                this->mixin(),
                state_spec<_TargetStateDef>{});
        }
    }

private:
    to_variant_t<mixin_t<SubStates>> active_sub_state_;
};


template <typename _StateDef>
class SimpleStateMixin : public StateMixin<_StateDef>
{
public:
    using typename StateMixin<_StateDef>::SuperStateMixin;

    template <typename _TargetStateSpec>
    SimpleStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec)
    : StateMixin<_StateDef>(super_state_mixin)
    {}

    template <typename _TargetStateDef>
    void executeTransition() { }
};

template <typename _StateDef>
class OrthogonalStateMixin : public StateMixin<_StateDef>
{
public:
    using Regions = typename _StateDef::Regions;
    using typename StateMixin<_StateDef>::TopStateDef;
    using typename StateMixin<_StateDef>::SuperStateMixin;

    template <typename _TargetStateSpec>
    OrthogonalStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec spec)
    : StateMixin<_StateDef>(super_state_mixin),
      regions_{init_regions(super_state_mixin, spec, state_spec<Regions>{})}
    {}

    template <typename ... _TargetStateSpec, typename ... _RegionDef>
    static auto init_regions(SuperStateMixin& super_state_mixin, std::tuple<_TargetStateSpec...> target_spec, state_spec_t<std::tuple<_RegionDef...>>) {
        return std::make_tuple(mixin_t<_RegionDef>{super_state_mixin, target_spec}...);
    }

    template <typename _Event>
    bool handleEvent(const _Event& e) {
        auto do_handle_event = [&](auto& ... region){ return (region.handleEvent(e) || ...); };
        auto regions_reaction_result = std::apply(do_handle_event, regions_);
        return regions_reaction_result->reacted ? regions_reaction_result : StateMixin<_StateDef>::template handleEvent<_Event>(e);
    }

    template <typename _TargetStateDef>
    void executeTransition() {
        auto do_execute_transition = [&](auto& ... region){ (region.template executeTransition<_TargetStateDef>(), ...); };
        std::apply(do_execute_transition, regions_);
    }

private:
    mixin_t<Regions> regions_;
};

template <typename _TopStateDef>
class TopState : public StateCrtp<_TopStateDef, _TopStateDef> , public TopStateBase
{};

template <typename _StateDef>
class CompositeTopStateMixin : public CompositeStateMixin<_StateDef>
{
public:
    CompositeTopStateMixin(StateMachine<_StateDef>& state_machine)
    : CompositeStateMixin<_StateDef>(*this, state_spec<initial_state_t<_StateDef>>{}),
      state_machine_{state_machine}
    {}

    template <typename _ContextDef>
    _ContextDef& context();

private:
    StateMachine<_StateDef>& state_machine_;
};

template <typename _StateDef>
class OrthogonalTopStateMixin : public OrthogonalStateMixin<_StateDef>
{
public:
    OrthogonalTopStateMixin(StateMachine<_StateDef>& state_machine)
    : OrthogonalStateMixin<_StateDef>(*this, state_spec<initial_state_t<_StateDef>>{}),
      state_machine_{state_machine}
    {}

    template <typename _ContextDef>
    _ContextDef& context();

private:
    StateMachine<_StateDef>& state_machine_;
};


//=====================================================================================================//
//                                         STATE MACHINE                                               //
//=====================================================================================================//

template <typename _TopStateDef>
class StateMachine
{
public:
    StateMachine()
    : top_state_(*this)
    {}

    template <typename _Event>
    bool dispatch(const _Event& event) {
        auto [any_state_reacted, transition] = top_state_.handleEvent(event);
        if(any_state_reacted && transition != nullptr) {
            transition(top_state_);
        }
        return any_state_reacted;
    }

private:
    mixin_t<_TopStateDef> top_state_;
};



}