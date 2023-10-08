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

template<typename _TopStateDef>
struct TypeErasedTransition
{   
    TypeErasedTransition(bool reacted)
    : reacted{reacted}
    {}
    bool reacted;
    virtual void execute(mixin_t<_TopStateDef>&) const {};
};

template <typename _TopStateDef, typename _TargetStateDef>
struct RegularTransition : public TypeErasedTransition<_TopStateDef>
{
    using TypeErasedTransition<_TopStateDef>::TypeErasedTransition;
    void execute(mixin_t<_TopStateDef>& top_state) const override {
        top_state.template executeTransition<_TargetStateDef>();
    }
};

template <typename _TopStateDef>
struct NoTransition : public TypeErasedTransition<_TopStateDef>
{
    using TypeErasedTransition<_TopStateDef>::TypeErasedTransition;
};

template <typename _TopStateDef, typename _TargetStateDef>
const RegularTransition<_TopStateDef, _TargetStateDef> regular_transition_{ true };

template <typename _TopStateDef>
const NoTransition<_TopStateDef> no_transition_{ true };

template <typename _TopStateDef>
const TypeErasedTransition<_TopStateDef> no_reaction_{ false };

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
    const TypeErasedTransition<TopStateDef>* transition() {
        //(static_assert(is_in_context_recursive_v<_TargetStateDef, TopStateDef>),...);
        return &regular_transition_<TopStateDef,_TargetStateDef>;
    }

    const TypeErasedTransition<TopStateDef>* no_transition() {
        return &no_transition_<TopStateDef>;
    }

    const TypeErasedTransition<TopStateDef>* condition_not_met() {
        return &no_reaction_<TopStateDef>;
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
    using TopStateDef = typename decltype(_StateDef::top_state_spec())::type;
    using SuperStateDef = super_state_t<_StateDef>;
    using SuperStateMixin = mixin_t<SuperStateDef>;

    StateMixin(SuperStateMixin& super_state_mixin)
    : super_state_mixin_{super_state_mixin}
    {}

    template <typename _Event>
    const TypeErasedTransition<TopStateDef>* handleEvent(const _Event& e) {
        if constexpr(has_reaction_to_event_v<_StateDef, _Event>) {
            return this->react(e);
        }
        return &no_reaction_<TopStateDef>;
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

protected:
    SuperStateMixin& super_state_mixin_;
};


template <typename _StateDef>
class CompositeStateMixin : public StateMixin<_StateDef>
{
public:
    using SubStates = typename _StateDef::SubStates;
    using typename StateMixin<_StateDef>::TopStateDef;
    using typename StateMixin<_StateDef>::SuperStateMixin;

    template <typename _TargetStateSpec, typename _DirectSubStateToEnter>
    CompositeStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec target_spec, state_spec<_DirectSubStateToEnter> enter_spec,
        std::enable_if_t<is_composite_state_v<_DirectSubStateToEnter>>* = nullptr)
    : StateMixin<_StateDef>(super_state_mixin),
      active_sub_state_{ 
        std::in_place_type<mixin_t<_DirectSubStateToEnter>>,
        this->mixin(),
        target_spec
      }
    {}

    template <typename _TargetStateSpec, typename _DirectSubStateToEnter>
    CompositeStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec target_spec, state_spec<_DirectSubStateToEnter> enter_spec,
        std::enable_if_t<is_simple_state_v<_DirectSubStateToEnter>>* = nullptr)
    : StateMixin<_StateDef>(super_state_mixin),
      active_sub_state_{ 
        std::in_place_type<mixin_t<_DirectSubStateToEnter>>,
        this->mixin()
      }
    {}

    template <typename _TargetStateSpec>
    CompositeStateMixin(SuperStateMixin& super_state_mixin, _TargetStateSpec spec)
    : CompositeStateMixin(super_state_mixin, spec, state_spec<direct_substate_to_enter_t<_StateDef, typename _TargetStateSpec::type>>{})
    {}

    template <typename _Event>
    const TypeErasedTransition<TopStateDef>* handleEvent(const _Event& e) {
        auto do_handle_event = [&](auto& active_sub_state){ return active_sub_state.handleEvent(e); };
        auto substate_reaction_result = std::visit(do_handle_event, active_sub_state_);
        return substate_reaction_result->reacted ? substate_reaction_result : StateMixin<_StateDef>::template handleEvent<_Event>(e);
    }

    template <typename _TargetStateDef>
    void executeTransition() {
        auto is_target_in_context = [&](auto& active_sub_state) { return is_in_context_recursive_v<_TargetStateDef, decltype(active_sub_state)>; };

        if(std::visit(is_target_in_context, active_sub_state_)) {
            auto do_execute_transition = [](auto& active_sub_state){ active_sub_state.template executeTransition<_TargetStateDef>(); };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            using Initial = direct_substate_to_enter_t<_StateDef, _TargetStateDef>;
            if constexpr(is_simple_state_v<Initial>) {
                active_sub_state_.template emplace<mixin_t<Initial>>(
                    this->mixin());
            }
            else {
                active_sub_state_.template emplace<mixin_t<Initial>>(
                    this->mixin(),
                    state_spec<_TargetStateDef>{});
            }
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

    SimpleStateMixin(SuperStateMixin& super_state_mixin)
    : StateMixin<_StateDef>(super_state_mixin)
    {}

    template <typename _TargetStateDef>
    void executeTransition() { }
};

template <typename _TopStateDef>
class TopState : public StateCrtp<_TopStateDef, _TopStateDef> , public TopStateBase
{};

template <typename _StateDef>
class TopStateMixin : public CompositeStateMixin<_StateDef>
{
public:
    TopStateMixin(StateMachine<_StateDef>& state_machine)
    : CompositeStateMixin<_StateDef>(*this, state_spec<std::tuple<initial_state_t<_StateDef>>>{}),
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
        auto reaction_result = top_state_.handleEvent(event);
        if(reaction_result->reacted) {
            reaction_result->execute(top_state_);
        }
        return true;
    }

private:
    TopStateMixin<_TopStateDef> top_state_;
};



}