#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

namespace metahsm {

template <typename StateDefinition, typename Event, typename Invocable = void>
struct HasReactionToEvent
{
    static inline constexpr bool value = false;
};

template <typename StateDefinition, typename Event>
struct HasReactionToEvent<StateDefinition, Event, std::is_invocable<decltype(&StateDefinition::react), Event>>
{
    static inline constexpr bool value = std::is_invocable_v<decltype(&StateDefinition::react), Event, Target>;
};

template <typename StateDefinition, typename SuperStateMixin>
class StateMixin;

template<typename SourceState>
class ReactResult
{
public:
    ReactResult(bool reaction_executed)
    : reaction_executed_{reaction_executed}
    {}

    virtual void executeTransition(SourceState& source) = 0;
    bool reaction_executed_;
};

template <typename SourceState, typename TargetState>
class RegularTransition : public ReactResult<SourceState>
{
public:
    RegularTransition()
    : ReactResult(true)
    {}

    void executeTransition(SourceState& source) override {
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

    void executeTransition(SourceState& source) override {
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

    void executeTransition(SourceState& source) override {
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
    using Type = typename Collapse<typename LCAImpl<StateDefinition1, StateDefinition2, ContextDefinition>::Type...>;
};

template <typename StateDefinition1, typename StateDefinition2, typename ContextDefinition>
struct LCAImpl<StateDefinition1, StateDefinition2, ContextDefinition, std::enable_if_t<
        !std::is_void_v<typename ContextDefinition::SubStates> &&
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
    const ReactResult<Mixin>& transition() {
        static_assert(IsValidTransition<StateDefinition, TargetStateDefinition>::value);
        return regular_transition_react_result_<Mixin, StateMixin<TargetStateDefinition, SMD>>;
    }

    void onEntry() { }
    void onExit() { }

private:
    decltype(auto) actual() {
        return static_cast<StateDefinition&>(*this);
    }

    decltype(auto) mixin() {
        return static_cast<Mixin>&>(*this);
    }
};

template <typename State>
struct StateSpec {
    using Type = State;
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
class SubState<StateDefinition, typename StateDefinition::SubStates>
{
public:
    using Default = std::tuple_element_t<0, typename StateDefinition::SubStates>;
    using Variant = typename StateVariant<typename StateDefinition::SubStates>::Type;
    using SMD = typename StateDefinition::SMD;

    template <typename Spec>
    SubState(StateMachineMixin<SMD>& state_machine_mixin, Spec spec)
    : active_sub_state_variant_{ 
        std::in_place_type<typename Variant::template ContainingState<typename Spec::Type>>,
        state_machine_mixin,
        spec
      }
    {}

    static inline constexpr bool defined = true;

    template <typename Event>
    decltype(auto) handleEvent(const Event& e) {
        return std::visit([](auto& active_sub_state){ return active_sub_state.handleEvent(); }, active_sub_state_variant_);
    }

    template <typename SourceState>
    void executeTransition(const ReactResult<SourceState>& react_result) {
        std::visit([](auto& active_sub_state){ active_sub_state.executeTransition(react_result); }, active_sub_state_variant_);
    }
private:
    Variant active_sub_state_variant_;
};

template <typename StateDefinition, typename StateMachineDefinition>
class StateMixin : public StateDefinition
{
    using SubState = SubState<StateDefinition>;
    using Initial = typename Collapse<typename StateDefinition::Initial, typename SubState::Default>::Type;
public:
    template <typename SubStateSpec>
    StateMixin(StateMachineMixin<StateMachineDefinition>& state_machine_mixin, SubStateSpec spec)
    : state_machine_mixin_{state_machine_mixin},
      active_sub_state_{spec}
    {}

    template <typename Event>
    decltype(auto) handleEvent(const Event& e) {
        if constexpr (SubState::defined) {
            decltype(auto) react_result = active_sub_state_.handleEvent(e);
            if(react_result.reaction_executed_) {
                return react_result;
            }
        }
        if constexpr(HasReactionToEvent<StateDefinition, Event>::value) {
            decltype(auto) react_result = this->react(e);
            if(react_result.reaction_executed_) {
                react_result.executeTransition(*this);
                return react_result;
            }
        }
        return no_reaction_react_result_<StateDefinition>;
    }

    template <typename TargetState>
    void executeTransition() {
        if constexpr (SubState::defined) {
            active_sub_state_.template executeTransition<TargetState>();
        }
        else {
            state_machine_mixin_.template executeTransition<StateMixin, TargetState>();
        }
    }

    template <typename SourceState, typename TargetState>
    void executeTransition() {
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