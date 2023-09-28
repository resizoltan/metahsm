#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

namespace metahsm {

class Entity {};

struct SimpleStateType {};
struct CompositeStateType {};
struct TopStateType {};

struct EntityBase {};
struct StateBase : EntityBase {};
struct SimpleStateBase : StateBase {};
struct CompositeStateBase : StateBase {};
struct TopStateBase : CompositeStateBase {};

template <typename _Entity, typename _TopStateDef, typename _SFINAE = void, typename _Enable = void>
struct MetaType
{
    using Type = SimpleStateType;
};

template <typename _Entity, typename _TopStateDef>
struct MetaType<_Entity, _TopStateDef, std::void_t<typename _Entity::SubStates>, 
    std::enable_if_t<std::is_base_of_v<StateBase, _Entity>
                 && !std::is_base_of_v<TopStateBase, _Entity>
    >>
{
    using Type = CompositeStateType;
};

template <typename _Entity, typename _TopStateDef>
struct MetaType<_Entity, _TopStateDef, std::void_t<typename _Entity::SubStates>, 
    std::enable_if_t<std::is_base_of_v<TopStateBase, _Entity>>>
{
    using Type = TopStateType;
};

template <typename _StateDef, typename _TopStateDef>
using meta_type_t = typename MetaType<_StateDef, _TopStateDef>::Type;

template <typename _StateDef, typename _TopStateDef, typename _MetaType>
class MixinImpl;

template <typename _StateDef, typename _TopStateDef>
using Mixin = typename MixinImpl<_StateDef, _TopStateDef, meta_type_t<_StateDef, _TopStateDef>>::Type;

template <typename _StateDef, typename _Event, typename _Enable = void>
struct HasReactionToEvent : public std::false_type
{};

template <typename _StateDef, typename _Event>
struct HasReactionToEvent<_StateDef, _Event, std::void_t<std::is_invocable<decltype(&_StateDef::react), _StateDef&, const _Event&>>>
: std::true_type
{};

template <typename ... _Tuple>
using TupleJoin = decltype(std::tuple_cat(std::declval<_Tuple>()...));

template<typename _SourceStateDef>
class TypeErasedTransition
{
public:
    virtual bool execute(Mixin<_SourceStateDef, typename _SourceStateDef::TopStateDef>& source) const = 0;
};

template <typename _SourceStateDef, typename _TargetStateDef>
class RegularTransition : public TypeErasedTransition<_SourceStateDef>
{
public:
    bool execute(Mixin<_SourceStateDef, typename _SourceStateDef::TopStateDef>& source) const override {
        source.template executeTransition<_TargetStateDef>();
        return true;
    }
};

template <typename _SourceStateDef>
class NoTransition : public TypeErasedTransition<_SourceStateDef>
{
public:
    bool execute(Mixin<_SourceStateDef, typename _SourceStateDef::TopStateDef>&) const override {
        return true;
    }
};

template <typename _SourceStateDef>
class ConditionNotMet : public TypeErasedTransition<_SourceStateDef>
{
public:
    bool execute(Mixin<_SourceStateDef, typename _SourceStateDef::TopStateDef>&) const override {
        return false;
    }
};

template <typename _SourceStateDef, typename _TargetStateDef>
const RegularTransition<_SourceStateDef, _TargetStateDef> regular_transition_;

template <typename _SourceStateDef>
const NoTransition<_SourceStateDef> no_transition_;

template <typename _SourceStateDef>
const ConditionNotMet<_SourceStateDef> condition_not_met_;

template <typename _StateDef, typename _ContextDef, typename Enable = void>
struct IsInContextRecursive : std::false_type
{};

template <typename _StateDef>
struct IsInContextRecursive<_StateDef, _StateDef, void> : std::true_type
{};

template <typename _StateDef, typename ... _ContextDef>
struct IsInContextRecursive<_StateDef, std::tuple<_ContextDef...>> :
std::disjunction<IsInContextRecursive<_StateDef, _ContextDef>...>
{};

template <typename _StateDef, typename _ContextDef>
struct IsInContextRecursive<_StateDef, _ContextDef, std::enable_if_t<
        !std::is_same_v<_StateDef, _ContextDef> &&
        !std::is_void_v<typename _ContextDef::SubStates>
    >>
: IsInContextRecursive<_StateDef, typename _ContextDef::SubStates>
{};

template <typename ... _StateDef>
struct Collapse;

template <>
struct Collapse<>
{
    using Type = void;
};

template <typename _StateDef>
struct Collapse<_StateDef>
{
    using Type = _StateDef;
};

template <typename ... _StateDef>
struct Collapse<void, _StateDef...>
{
    using Type = typename Collapse<_StateDef...>::Type;
};

template <typename _StateDef, typename ... Us>
struct Collapse<_StateDef, Us...>
{
    using Type = _StateDef;
};

template <typename _StateDef1, typename _StateDef2, typename _ContextDef, typename _Enable = void>
struct LCAImpl
{
    using Type = void;
};

template <typename _StateDef, typename _ContextDef>
struct LCAImpl<_StateDef, _StateDef, _ContextDef, void>
{
    using Type = _StateDef;
};

template <typename _StateDef1, typename _StateDef2, typename ... _ContextDef>
struct LCAImpl<_StateDef1, _StateDef2, std::tuple<_ContextDef...>, void>
{
    using Type = typename Collapse<typename LCAImpl<_StateDef1, _StateDef2, _ContextDef>::Type...>::Type;
};

template <typename _StateDef1, typename _StateDef2, typename _ContextDef>
struct LCAImpl<_StateDef1, _StateDef2, _ContextDef, std::enable_if_t<
        !std::is_void_v<typename _ContextDef::SubStates> &&
        !std::is_same_v<_StateDef1, _StateDef2> &&
        IsInContextRecursive<_StateDef1, _ContextDef>::value &&
        IsInContextRecursive<_StateDef2, _ContextDef>::value
    >>
{
    using SubType = typename LCAImpl<_StateDef1, _StateDef2, typename _ContextDef::SubStates>::Type;
    using Type = typename Collapse<SubType, _ContextDef>::Type;
};

template <typename _StateDef1, typename _StateDef2>
using LCA = typename LCAImpl<_StateDef1, _StateDef2, typename _StateDef1::TopStateDef>::Type;

template <typename _StateDef, typename _ContextDef, typename _ContextDef, typename Enable = void>
struct ContextImpl
{
    using Type = void;
};

template <typename _StateDef, typename ... _StateDefToCompare, typename _ContextDef>
struct ContextImpl<_StateDef, std::tuple<_StateDefToCompare...>, _ContextDef, void>
{
    using Type = typename Collapse<typename ContextImpl<_StateDef, _StateDefToCompare, _ContextDef>::Type...>::Type;
};

template <typename _StateDef, typename _StateDefToCompare, typename _ContextDef>
struct ContextImpl<_StateDef, _StateDefToCompare, typename _ContextDef, std::enable_if_t<
        !std::is_void_v<typename _StateDefToCompare::SubStates> &&
        !std::is_same_v<_StateDef, _StateDefToCompare> &&
        IsInContextRecursive<_StateDef, _StateDefToCompare>::value
    >>
{
    using SubType = typename ContextImpl<_StateDef, typename _StateDefToCompare::SubStates, _StateDefToCompare>::Type;
    using Type = typename Collapse<SubType, _StateDefToCompare>::Type;
};

template <typename _StateDef, typename _ContextDef>
struct ContextImpl<_StateDef, _StateDef, _ContextDef, void>
{
    using Type = _ContextDef;
};

template <typename _StateDef, typename _TopStateDef>
using Context  = typename ContextImpl<_StateDef, _TopStateDef, _TopStateDef>::Type;

template <typename _SourceStateDef, typename _TargetStateDef>
struct IsValidTransition
{
    static constexpr bool value = IsInContextRecursive<_TargetStateDef, typename _SourceStateDef::TopStateDef>::value;
};

template <typename _StateDef, typename _TopStateDef>
class StateCrtp : public StateBase
{
public:
    template <typename _SubStateDef>
    using State = StateCrtp<_SubStateDef, _TopStateDef>;
    using TopStateDef = _TopStateDef;
    using Initial = void;

    StateCrtp() {
        onEntry();
    }

    ~StateCrtp() {
        onExit();
    }

    template <typename _ContextDef>
    decltype(auto) context()  {
        static_assert(IsInContextRecursive<_StateDef, _ContextDef>::value, "Requested context is not available in the state!");
        return mixin().template context<_ContextDef>();
    }

    template <typename _TargetStateDef>
    const TypeErasedTransition<_StateDef>* transition() {
        static_assert(IsValidTransition<_StateDef, _TargetStateDef>::value);
        return &regular_transition_<_StateDef, _TargetStateDef>;
    }

    const TypeErasedTransition<_StateDef>* no_transition() {
        return &no_transition_<_StateDef>;
    }

    const TypeErasedTransition<_StateDef>* condition_not_met() {
        return &condition_not_met_<_StateDef>;
    }

    void onEntry() { }
    void onExit() { }

private:
    decltype(auto) mixin() {
        return static_cast<Mixin<_StateDef, _TopStateDef>&>(*this);
    }
};

template <typename _StateDef>
struct StateSpec
{
    using StateDef = _StateDef;
};

template <typename _StateDefTuple, typename _TopStateDef>
class StateTuple;

template <typename ... _StateDef, typename _TopStateDef>
struct StateTuple<std::tuple<_StateDef...>, _TopStateDef>
{
public:
    using Tuple = std::tuple<Mixin<_StateDef, _TopStateDef>...>;
    using Variant = std::variant<Mixin<_StateDef, _TopStateDef>...>;
    using Default = std::tuple_element_t<0, std::tuple<_StateDef...>>;
    template <typename _SubStateDef>
    using ContainingState = typename Collapse<
        std::conditional_t<
            IsInContextRecursive<_SubStateDef, StateDefinition>::value,
            StateDefinition,
            void
        >...>::Type;
};


template <typename _StateDef>
class TopStateMixin;

template <typename _StateDef, typename _TopStateDef>
class StateMixin : public _StateDef
{
public:
    using TopStateMixin = TopStateMixin<_TopStateDef>;
    using ContextMixin = Mixin<Context<_StateDef, _TopStateDef>, _TopStateDef>;

    StateMixin(TopStateMixin& top_state_mixin, ContextMixin& context_mixin)
    : top_state_mixin_{top_state_mixin},
      context_mixin_{context_mixin}
    {}

    template <typename _Event>
    bool handleEvent(const _Event& e) {
        if constexpr(HasReactionToEvent<_StateDef, _Event>::value) {
            auto type_erased_transition = this->react(e);
            return type_erased_transition->execute(mixin());
        }
        return false;
    }

    template <typename _ContextDef>
    _ContextDef& context() {
        if constexpr(std::is_same_v<_ContextDef, _StateDef>) {
            return *this;
        }
        else {
            return context_mixin_.template context<_ContextDef>();
        }
    }

protected:
    TopStateMixin& top_state_mixin_;
    ContextMixin& context_mixin_;

    decltype(auto) mixin() {
        return static_cast<Mixin<_StateDef, _TopStateDef>&>(*this);
    }
};

template <typename _StateDef, typename _TopStateDef>
class CompositeStateMixin : public StateMixin<_StateDef, _TopStateDef>
{
public:
    using SubStates = StateTuple<typename _StateDef::SubStates, _TopStateDef>;
    using Initial = typename Collapse<typename _StateDef::Initial, typename SubStates::Default>::Type;

    template <typename _SubStateSpec>
    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin,
        _SubStateSpec spec,
        std::enable_if_t<std::is_same_v<meta_type_t<typename _SubStateSpec::StateDef, _TopStateDef>, CompositeStateType>>* = nullptr)
    : StateMixin(top_state_mixin, context_mixin),
      active_sub_state_{ 
        std::in_place_type<Mixin<typename SubStates::ContainingState<typename _SubStateSpec::StateDef>, _TopStateDef>>,
        top_state_mixin,
        mixin(),
        spec
      }
    {}

    template <typename _SubStateSpec>
    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin,
        _SubStateSpec spec,
        std::enable_if_t<std::is_same_v<meta_type_t<typename _SubStateSpec::StateDef, _TopStateDef>, SimpleStateType>>* = nullptr)
    : StateMixin(top_state_mixin, context_mixin),
      active_sub_state_{ 
        std::in_place_type<Mixin<typename SubStates::ContainingState<typename _SubStateSpec::StateDef>, _TopStateDef>>,
        top_state_mixin,
        mixin()
      }
    {}

    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin,
        StateSpec<_StateDef> spec)
    : CompositeStateMixin(top_state_mixin, context_mixin, StateSpec<Initial>{})
    {}

    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin)
    : CompositeStateMixin(top_state_mixin, context_mixin, StateSpec<Initial>{})
    {}

    template <typename _Event>
    bool handleEvent(const _Event& e) {
        auto do_handle_event = [&](auto& active_sub_state){ return active_sub_state.handleEvent(e); };
        bool substate_handled_the_event = std::visit(do_handle_event, active_sub_state_);
        if(substate_handled_the_event) {
            return true;
        }
        else {
            return StateMixin::handleEvent<Event>(e);
        }
    }

    template <typename _TargetStateDef>
    void executeTransition() {
        auto do_execute_transition = [](auto& active_sub_state){ active_sub_state.template executeTransition<_TargetStateDef>(); };
        std::visit(do_execute_transition, active_sub_state_);       
    }

    template <typename _LCA, typename _TargetStateDef>
    void executeTransition() {
        if constexpr (std::is_same_v<_LCA, _StateDef>) {
            using Spec = StateSpec<_TargetStateDef>;
            using SubStateDefinition = SubStates::ContainingState<typename Spec::StateDef>;
            active_sub_state_.emplace<Mixin<SubStateDefinition, typename SubStateDefinition::TopStateDef>>(
                top_state_mixin_,
                mixin());
        }
        else {
            auto do_execute_transition = [](auto& active_sub_state){ active_sub_state.template executeTransition<_LCA, _TargetStateDef>(); };
            std::visit(do_execute_transition, active_sub_state_);       
        }
    }

private:
    typename SubStates::Variant active_sub_state_;
};

template <typename _StateDef, typename _TopStateDef>
class SimpleStateMixin : public StateMixin<_StateDef, _TopStateDef>
{
public:
    SimpleStateMixin(TopStateMixin& top_state_mixin, ContextMixin& context_mixin)
    : StateMixin(top_state_mixin, context_mixin)
    {}

    template <typename _TargetStateDef>
    void executeTransition() {
        using LCA = LCA<_StateDef, _TargetStateDef>;
        top_state_mixin_.template executeTransition<LCA, _TargetStateDef>();             
    }

    template <typename _LCA, typename _TargetStateDef>
    void executeTransition() { }
};

template <typename _TopStateDef>
class TopState : public StateCrtp<_TopStateDef, _TopStateDef> , public TopStateBase
{};

template <typename _TopStateDef>
class StateMachine;

template <typename _StateDef>
class TopStateMixin : public CompositeStateMixin<_StateDef, _StateDef>
{
public:
    TopStateMixin(StateMachine<_StateDef>& state_machine)
    : CompositeStateMixin(*this, *this),
      state_machine_{state_machine}
    {}

    template <typename _ContextDef>
    _ContextDef& context();

private:
    StateMachine<_StateDef>& state_machine_;
};

template <typename _StateDef, typename _TopStateDef, typename _MetaType>
struct MixinImpl;

template <typename _StateDef, typename _TopStateDef>
struct MixinImpl<_StateDef, _TopStateDef, SimpleStateType>
{
    using Type = SimpleStateMixin<_StateDef, _TopStateDef>;
};

template <typename _StateDef, typename _TopStateDef>
struct MixinImpl<_StateDef, _TopStateDef, CompositeStateType>
{
    using Type = CompositeStateMixin<_StateDef, _TopStateDef>;
};

template <typename _StateDef>
struct MixinImpl<_StateDef, _StateDef, TopStateType>
{
    using Type = TopStateMixin<_StateDef>;
};


}