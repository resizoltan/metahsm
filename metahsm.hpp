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

    static TopStateDef top_state_def();
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
    using TopStateDef = decltype(_StateDef::top_state_def());
    using SuperStateDef = super_state_t<_StateDef>;
    using SuperStateMixin = mixin_t<SuperStateDef>;

    struct Initializer
    {
        SuperStateMixin& super_state_mixin;
        std::size_t target_combination;
    };

    StateMixin(Initializer initializer)
    : super_state_mixin_{initializer.super_state_mixin}
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
        this->target_combination_ = 0;
        if constexpr(has_reaction_to_event_v<_StateDef, _Event>) {
            return std::make_tuple(this->react(e), this->target_combination_);
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
        target_combination_ = state_combination_v<_TargetStateDef>;
    }

protected:
    SuperStateMixin& super_state_mixin_;
    std::size_t target_combination_;
};


template <typename _StateDef>
class CompositeStateMixin : public StateMixin<_StateDef>
{
public:
    using SubStates = typename _StateDef::SubStates;
    using typename StateMixin<_StateDef>::TopStateDef;
    using typename StateMixin<_StateDef>::SuperStateMixin;
    using typename StateMixin<_StateDef>::Initializer;

    CompositeStateMixin(Initializer initializer)
    : StateMixin<_StateDef>(initializer)
    {
        std::size_t substate_to_enter_local_id = direct_substate_to_enter_f(initializer.target_combination, type_identity<SubStates>{});
        std::invoke(lookup_table[substate_to_enter_local_id], this, initializer.target_combination);
    }

    template <typename _DirectSubStateToEnter>
    void enter_substate(std::size_t target_combination){
        using T = mixin_t<_DirectSubStateToEnter>;
        active_sub_state_.template emplace<T>(typename T::Initializer{this->mixin(), target_combination});
        active_state_id_ = state_id_v<_DirectSubStateToEnter>;
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
        remove_conflicting(target_combination, type_identity<SubStates>{});
        auto id = state_combination_v<std::tuple_element_t<0, SubStates>>;
        bool is_target_in_context = static_cast<bool>(target_combination & (1 << active_state_id_))
            || (!static_cast<bool>(target_combination & state_combination_v<SubStates>) && initial_state_id_ == active_state_id_);

        if(is_target_in_context) {
            auto do_execute_transition = overload{
                [&](auto& active_sub_state){ active_sub_state.executeTransition(target_combination); },
                [](std::monostate) { }
            };
            std::visit(do_execute_transition, active_sub_state_); 
        }
        else {
            std::size_t substate_to_enter_local_id = direct_substate_to_enter_f(target_combination, type_identity<SubStates>{});
            std::invoke(lookup_table[substate_to_enter_local_id], this, target_combination);
        }
    }

private:
    template <typename ... _SubStateDef>
    static constexpr auto init_lookup_table(type_identity<std::tuple<_SubStateDef...>>) {
        return std::array{&enter_substate<_SubStateDef>..., &enter_substate<initial_state_t<_StateDef>>};
    }

    to_variant_t<tuple_join_t<std::monostate, mixins_t<SubStates>>> active_sub_state_;
    std::size_t active_state_id_;
    static constexpr std::size_t initial_state_id_ = state_id_v<initial_state_t<_StateDef>>;
    static constexpr std::array<void(CompositeStateMixin<_StateDef>::*)(std::size_t), std::tuple_size_v<SubStates> + 1> lookup_table = init_lookup_table(type_identity<SubStates>{});
};


template <typename _StateDef>
class SimpleStateMixin : public StateMixin<_StateDef>
{
public:
    using typename StateMixin<_StateDef>::SuperStateMixin;
    using typename StateMixin<_StateDef>::Initializer;

    SimpleStateMixin(Initializer initializer)
    : StateMixin<_StateDef>(initializer)
    {}

    void executeTransition(std::size_t) { }
};

template <typename _StateDef>
class OrthogonalStateMixin : public StateMixin<_StateDef>
{
public:
    using Regions = reverse_tuple_t<typename _StateDef::Regions>;
    using typename StateMixin<_StateDef>::TopStateDef;
    using typename StateMixin<_StateDef>::SuperStateMixin;
    using typename StateMixin<_StateDef>::Initializer;

    template <std::size_t ... I>
    OrthogonalStateMixin(Initializer initializer, std::index_sequence<I...>)
    : StateMixin<_StateDef>(initializer),
      regions_{typename mixin_t<std::tuple_element_t<I, Regions>>::Initializer{this->mixin(), initializer.target_combination}...} 
    {}

    OrthogonalStateMixin(Initializer initializer)
    : OrthogonalStateMixin<_StateDef>(initializer, std::make_index_sequence<std::tuple_size_v<Regions>>{})
    {}


    template <typename _Event>
    auto handleEvent(const _Event& e) {
        auto do_handle_event = [&](auto& ... region){ return (region.handleEvent(e) + ...); };
        auto [substate_reacted, transition] = std::apply(do_handle_event, regions_);
        return substate_reacted ? std::make_tuple(substate_reacted, transition) : StateMixin<_StateDef>::template handleEvent<_Event>(e);
    }

    void executeTransition(std::size_t target_combination) {
        auto do_execute_transition = [&](auto& ... region){ (region.executeTransition(target_combination), ...); };
        std::apply(do_execute_transition, regions_);
    }

private:
    mixins_t<Regions> regions_;
};



class RootStateMixin
{};

template <typename _TopStateDef>
class TopState : public StateCrtp<_TopStateDef, _TopStateDef> , public TopStateBase
{};

template <typename _StateMixin>
class TopStateMixin : public _StateMixin
{
    using typename _StateMixin::StateDef;
    using typename _StateMixin::Initializer;
public:
    TopStateMixin(StateMachine<StateDef>& state_machine)
    : _StateMixin(Initializer{root_state_mixin_, state_combination_recursive_v<initial_state_t<StateDef>>}),
      state_machine_{state_machine}
    {}

    template <typename _ContextDef>
    _ContextDef& context();

private:
    StateMachine<StateDef>& state_machine_;
    RootStateMixin root_state_mixin_;
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
        auto [any_state_reacted, target_combination] = top_state_.handleEvent(event);
        if(any_state_reacted && target_combination != 0) {
            top_state_.executeTransition(target_combination);
        }
        return any_state_reacted;
    }

private:
    mixin_t<_TopStateDef> top_state_;
};



}