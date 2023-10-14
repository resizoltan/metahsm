#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

#include "type_traits.hpp"

namespace metahsm {

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
        this->mixin().template set_transition<_TargetStateDef>();
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
        this->target_state_id_ = 0;
        if constexpr(has_reaction_to_event_v<_StateDef, _Event>) {
            return std::make_tuple(this->react(e), this->target_state_id_);
        }
        else {
            return std::make_tuple(false, (std::size_t)0);
        }
    }

    template <typename _ContextDef>
    auto& context() {
        if constexpr(std::is_same_v<_ContextDef, _StateDef>) {
            return *this;
        }
        else {
            return super_state_mixin_.template context<_ContextDef>();
        }
    }

    template <typename _TargetStateDef>
    void set_transition() {
        target_state_id_ = state_id_v<_TargetStateDef>;
    }

protected:
    SuperStateMixin& super_state_mixin_;
    std::size_t target_state_id_;
};


template <typename _StateDef>
class CompositeStateMixin : public StateMixin<_StateDef>
{
public:
    using SubStates = typename _StateDef::SubStates;
    using typename StateMixin<_StateDef>::TopStateDef;
    using typename StateMixin<_StateDef>::SuperStateMixin;

    CompositeStateMixin(SuperStateMixin& super_state_mixin, std::size_t target_combination)
    : StateMixin<_StateDef>(super_state_mixin)
    {
        std::size_t substate_to_enter_local_id = direct_substate_to_enter_f(target_combination, state_spec<SubStates>{});
        std::invoke(lookup_table[substate_to_enter_local_id], this, target_combination);
    }

    template <typename _DirectSubStateToEnter>
    void enter_substate(std::size_t target_combination){
         active_sub_state_.template emplace<mixin_t<_DirectSubStateToEnter>>(
            this->mixin(),
            target_combination);
    }

    template <typename _Event>
    auto handleEvent(const _Event& e) {
        auto do_handle_event = overload{
            [&](auto& active_sub_state){ return active_sub_state.handleEvent(e); },
            [](std::monostate){ return std::make_tuple(false, (std::size_t)0); }
        };
        auto [substate_reacted, transition] = std::visit(do_handle_event, active_sub_state_);
        return substate_reacted ? std::make_tuple(substate_reacted, transition) : StateMixin<_StateDef>::template handleEvent<_Event>(e);
    }

    void executeTransition(std::size_t target_combination) {
        auto is_target_in_context = overload{
            [&](auto& active_sub_state) { 
                return target_combination & state_combination_v<typename std::remove_reference_t<decltype(active_sub_state)>::StateDef>; 
            },
            [](std::monostate) { return (std::size_t)0; }
        };

        if(std::visit(is_target_in_context, active_sub_state_)) {
            auto do_execute_transition = overload{
                [&](auto& active_sub_state){ active_sub_state.executeTransition(target_combination); },
                [](std::monostate) { }
            };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            std::size_t substate_to_enter_local_id = direct_substate_to_enter_f(target_combination, state_spec<SubStates>{});
            std::invoke(lookup_table[substate_to_enter_local_id], this, target_combination);
        }
    }

private:
    template <typename ... _SubStateDef>
    static constexpr auto init_lookup_table(state_spec<std::tuple<_SubStateDef...>>) {
        return std::array{&enter_substate<_SubStateDef>..., &enter_substate<initial_state_t<_StateDef>>};
    }

    to_variant_t<tuple_add_t<std::monostate, mixin_t<SubStates>>> active_sub_state_;
    static constexpr std::array<void(CompositeStateMixin<_StateDef>::*)(std::size_t), std::tuple_size_v<SubStates> + 1> lookup_table = init_lookup_table(state_spec<SubStates>{});
};


template <typename _StateDef>
class SimpleStateMixin : public StateMixin<_StateDef>
{
public:
    using typename StateMixin<_StateDef>::SuperStateMixin;

    SimpleStateMixin(SuperStateMixin& super_state_mixin, std::size_t)
    : StateMixin<_StateDef>(super_state_mixin)
    {}

    void executeTransition(std::size_t) { }
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
    : CompositeStateMixin<_StateDef>(*this, 1 << state_id_v<initial_state_t<_StateDef>>),
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
        auto [any_state_reacted, target_state_id] = top_state_.handleEvent(event);
        if(any_state_reacted && target_state_id != 0) {
            top_state_.executeTransition(1 << target_state_id);
        }
        return any_state_reacted;
    }

private:
    mixin_t<_TopStateDef> top_state_;
};



}