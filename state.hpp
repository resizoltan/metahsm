#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

namespace metahsm {

template <typename StateDefinition, typename Event, typename Invocable = void>
struct HasReactionToEvent : public std::false_type
{};

template <typename StateDefinition, typename Event>
struct HasReactionToEvent<StateDefinition, Event, std::void_t<std::is_invocable<decltype(&StateDefinition::react), StateDefinition&, const Event&>>>
: std::true_type
{};

template <typename StateDefinition, typename SuperStateMixin>
class StateMixin;

class ReactResultBase
{
public:
    ReactResultBase(bool reaction_executed)
    : reaction_executed_{reaction_executed}
    {}
    bool reaction_executed_;
};

template<typename SourceState>
class ReactResult : public ReactResultBase
{
public:
    using ReactResultBase::ReactResultBase;

    virtual void executeTransition(SourceState& source) const = 0;
};

template <typename SourceState, typename TargetState>
class RegularTransition : public ReactResult<SourceState>
{
public:
    RegularTransition()
    : ReactResult(true)
    {}

    void executeTransition(SourceState& source) const override {
        source.template executeTransition<TargetState>();
    }
};

template <typename SourceState>
class NoTransition : public ReactResult<SourceState>
{
public:
    NoTransition()
    : ReactResult(true)
    {}

    void executeTransition(SourceState& source) const override {
        (void)source;
    }
};

template <typename SourceState>
class NoReactionTransition : public ReactResult<SourceState>
{
public:
    NoReactionTransition()
    : ReactResult(false)
    {}

    void executeTransition(SourceState& source) const override {
        (void)source;
    }
};

template <typename SourceState, typename TargetState>
const RegularTransition<SourceState, TargetState> regular_transition_react_result_;

template <typename SourceState>
const NoTransition<SourceState> no_transition_react_result_;

template <typename SourceState>
const NoReactionTransition<SourceState> no_reaction_react_result_;

template <typename StateDefinition, typename ContextDefinition, typename Enable = void>
struct IsInContext : std::false_type
{};

template <typename StateDefinition>
struct IsInContext<StateDefinition, StateDefinition, void> : std::true_type
{};

template <typename StateDefinition, typename ... ContextDefinition>
struct IsInContext<StateDefinition, std::tuple<ContextDefinition...>> :
std::disjunction<IsInContext<StateDefinition, ContextDefinition>...>
{};

template <typename StateDefinition, typename ContextDefinition>
struct IsInContext<StateDefinition, ContextDefinition, std::enable_if_t<
        !std::is_same_v<StateDefinition, ContextDefinition> &&
        !std::is_void_v<typename ContextDefinition::SubStates>
    >>
: IsInContext<StateDefinition, typename ContextDefinition::SubStates>
{};

template <typename ... T>
struct Collapse;

template <>
struct Collapse<>
{
    using Type = void;
};

template <typename T>
struct Collapse<T>
{
    using Type = T;
};

template <typename ... T>
struct Collapse<void, T...>
{
    using Type = typename Collapse<T...>::Type;
};

template <typename T, typename ... Us>
struct Collapse<T, Us...>
{
    using Type = T;
};

template <typename StateDefinition1, typename StateDefinition2, typename ContextDefinition, typename Enable = void>
struct LCAImpl
{
    using Type = void;
};

template <typename StateDefinition, typename ContextDefinition>
struct LCAImpl<StateDefinition, StateDefinition, ContextDefinition, void>
{
    using Type = StateDefinition;
};

template <typename StateDefinition1, typename StateDefinition2, typename ... ContextDefinition>
struct LCAImpl<StateDefinition1, StateDefinition2, std::tuple<ContextDefinition...>, void>
{
    using Type = typename Collapse<typename LCAImpl<StateDefinition1, StateDefinition2, ContextDefinition>::Type...>::Type;
};

template <typename StateDefinition1, typename StateDefinition2, typename ContextDefinition>
struct LCAImpl<StateDefinition1, StateDefinition2, ContextDefinition, std::enable_if_t<
        !std::is_void_v<typename ContextDefinition::SubStates> &&
        !std::is_same_v<StateDefinition1, StateDefinition2> &&
        IsInContext<StateDefinition1, ContextDefinition>::value &&
        IsInContext<StateDefinition2, ContextDefinition>::value
    >>
{
    using SubType = typename LCAImpl<StateDefinition1, StateDefinition2, typename ContextDefinition::SubStates>::Type;
    using Type = typename Collapse<SubType, ContextDefinition>::Type;
};

template <typename StateDefinition1, typename StateDefinition2>
using LCA = typename LCAImpl<StateDefinition1, StateDefinition2, typename StateDefinition1::SMD>::Type;

template <typename SourceStateDefinition, typename TargetStateDefinition>
struct IsValidTransition
{
    static constexpr bool value = IsInContext<TargetStateDefinition, typename SourceStateDefinition::SMD>::value;
};

template <typename StateDefinition, typename StateMachineDefinition>
class StateCrtp
{
public:
    template <typename SubStateDefinition>
    using State = StateCrtp<SubStateDefinition, StateMachineDefinition>;
    using SMD = StateMachineDefinition;
    using Mixin = StateMixin<StateDefinition, StateMachineDefinition>;
    using Initial = void;

    StateCrtp() {
        onEntry();
    }

    ~StateCrtp() {
        onExit();
    }

    template <typename ContextDefinition>
    decltype(auto) context()  {
        if constexpr(std::is_same_v<StateDefinition, ContextDefinition>) {
            return actual();
        }
        else {
            return mixin().template context<ContextDefinition>();
        }
    }

    template <typename TargetStateDefinition>
    const ReactResult<Mixin>* transition() {
        static_assert(IsValidTransition<StateDefinition, TargetStateDefinition>::value);
        return &regular_transition_react_result_<Mixin, StateMixin<TargetStateDefinition, SMD>>;
    }

    void onEntry() { }
    void onExit() { }

private:
    decltype(auto) actual() {
        return static_cast<StateDefinition&>(*this);
    }

    decltype(auto) mixin() {
        return static_cast<Mixin&>(*this);
    }
};

template <typename StateDefinition>
struct StateSpec {
    using Type = StateDefinition;
};

template <typename StateDefinitions>
class StateVariant;

template <typename ... StateDefinition>
struct StateVariant<std::tuple<StateDefinition...>>
{
public:
    using Type = std::variant<typename StateDefinition::Mixin...>;
    template <typename SubState>
    using ContainingState = typename Collapse<
        std::conditional_t<
            IsInContext<SubState, StateDefinition>::value,
            StateDefinition,
            void
        >...>::Type;
};

template <typename StateMachineDefinition>
class StateMachineMixin;

template <typename StateDefinition, typename HasSubstates = void> 
class SubState
{
public:
    using Default = void;
    using SMD = typename StateDefinition::SMD;

    template <typename Spec>
    SubState(StateMachineMixin<SMD>& state_machine_mixin, Spec spec)
    {}
    static inline constexpr bool defined = false;
};

template <typename StateDefinition>
class SubState<StateDefinition, std::void_t<typename StateDefinition::SubStates>>
{
public:
    using Default = std::tuple_element_t<0, typename StateDefinition::SubStates>;
    using Variant = StateVariant<typename StateDefinition::SubStates>;
    using SMD = typename StateDefinition::SMD;

    template <typename Spec>
    SubState(StateMachineMixin<SMD>& state_machine_mixin, Spec spec)
    : state_machine_mixin_{state_machine_mixin},
      active_sub_state_variant_{ 
        std::in_place_type<typename Variant::ContainingState<typename Spec::Type>::Mixin>,
        state_machine_mixin, spec
      }
    {}

    template <typename LCA, typename TargetStateDefinition>
    void executeTransition()
    {
        if constexpr(std::is_same_v<LCA, StateDefinition>) {
            using Spec = StateSpec<TargetStateDefinition>;
            active_sub_state_variant_.emplace<typename Variant::ContainingState<typename Spec::Type>::Mixin>(
                state_machine_mixin_,
                Spec{});
        }
        else {
            std::visit([](auto& active_sub_state){ 
                active_sub_state.template executeTransition<LCA, TargetStateDefinition>(); }, active_sub_state_variant_);
        }
    }

    static inline constexpr bool defined = true;

    template <typename Event>
    bool handleEvent(const Event& e) {
        return std::visit([&](auto& active_sub_state){ return active_sub_state.handleEvent(e); }, active_sub_state_variant_);
    }

    template <typename SourceState>
    void executeTransition(const ReactResult<SourceState>& react_result) {
        std::visit([](auto& active_sub_state){ active_sub_state.executeTransition(react_result); }, active_sub_state_variant_);
    }
private:
    StateMachineMixin<SMD>& state_machine_mixin_;
    typename Variant::Type active_sub_state_variant_;
};

template <typename StateDefinition, typename StateMachineDefinition>
class StateMixin : public StateDefinition
{
    using SubState = SubState<StateDefinition>;
    using Initial = typename Collapse<typename StateDefinition::Initial, typename SubState::Default>::Type;
    static constexpr bool is_composite = SubState::defined;
public:
    using Definition = StateDefinition;
    template <typename SubStateSpec>
    StateMixin(StateMachineMixin<StateMachineDefinition>& state_machine_mixin, SubStateSpec spec)
    : state_machine_mixin_{state_machine_mixin},
      active_sub_state_{state_machine_mixin, spec}
    {}

    template <>
    StateMixin(StateMachineMixin<StateMachineDefinition>& state_machine_mixin, StateSpec<StateDefinition> spec)
    : state_machine_mixin_{state_machine_mixin},
      active_sub_state_{state_machine_mixin, StateSpec<Initial>{}}
    {}

    template <typename Event>
    bool handleEvent(const Event& e) {
        if constexpr (is_composite) {
            bool reaction_executed = active_sub_state_.handleEvent(e);
            if(reaction_executed) {
                return true;
            }
        }
        if constexpr(HasReactionToEvent<StateDefinition, Event>::value) {
            auto react_result = this->react(e);
            if(react_result->reaction_executed_) {
                react_result->executeTransition(*this);
                return true;
            }
        }
        return false;
    }

    template <typename TargetState>
    void executeTransition() {
        if constexpr (is_composite) {
            active_sub_state_.template executeTransition<TargetState>();
        }
        else {
            using TargetStateDefinition = typename TargetState::Definition;
            using LCA = metahsm::LCA<StateDefinition, TargetStateDefinition>;
            state_machine_mixin_.template executeTransition<LCA, TargetStateDefinition>();
        }
    }

    template <typename LCA, typename TargetStateDefinition>
    void executeTransition() {
        if constexpr (is_composite) {
            active_sub_state_.template executeTransition<LCA, TargetStateDefinition>();
        }
    }

    template <typename ContextDefinition>
    void context() {
        if constexpr (SubState::defined) {
            //active_sub_state_.template executeTransition<TargetState>();
        }
        else {
            //using LCA = typename LCA<StateDefinition, TargetStateDefinition>::Type;
        }
    }

private:
    StateMachineMixin<StateMachineDefinition>& state_machine_mixin_;
    SubState active_sub_state_;
};

}