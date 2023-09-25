#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

namespace metahsm {

template <typename StateDefinition, typename Event, typename Invocable = void>
struct HasReactionToEvent {
    static inline constexpr bool value = false;
};

template <typename StateDefinition, typename Event>
struct HasReactionToEvent<StateDefinition, Event, std::is_invocable<decltype(&StateDefinition::react), Event>> {
    static inline constexpr bool value = std::is_invocable_v<decltype(&StateDefinition::react), Event, Target>;
};

template <typename StateDefinition, typename SuperStateDefinition>
class StateMixin;

template<typename SourceStateDefinition, typename SuperStateDefinition>
class ReactResult
{
public:
    ReactResult(bool reaction_executed)
    : reaction_executed_{reaction_executed}
    {}

    virtual void executeTransition(StateMixin<SourceStateDefinition, SuperStateDefinition>& source) = 0;
    bool reaction_executed_;
};

template <typename SourceStateDefinition, typename TargetStateDefinition, typename SuperStateDefinition>
class RegularTransition : public ReactResult<SourceStateDefinition, SuperStateDefinition> {
public:
    RegularTransition()
    : ReactResult(true)
    {}

    void executeTransition(StateMixin<SourceStateDefinition, SuperStateDefinition>& source) override {
        source.template executeTransition<TargetStateDefinition>();
    }
};

template <typename SourceStateDefinition, typename SuperStateDefinition>
class NoTransition : public ReactResult<SourceStateDefinition, SuperStateDefinition>
{
public:
    NoTransition()
    : ReactResult(true)
    {}

    void executeTransition(StateMixin<SourceStateDefinition, SuperStateDefinition>& source) override {
        (void)source;
    }
};

template <typename SourceStateDefinition, typename SuperStateDefinition>
class NoReactionTransition : public ReactResult<SourceStateDefinition, SuperStateDefinition>
{
public:
    NoReactionTransition()
    : ReactResult(false)
    {}

    void executeTransition(StateMixin<SourceStateDefinition, SuperStateDefinition>& source) override {
        (void)source;
    }
};

template <typename SourceStateDefinition, typename TargetStateDefinition, typename SuperStateDefinition>
const RegularTransition<SourceStateDefinition, TargetStateDefinition, SuperStateDefinition> regular_transition_react_result_;

template <typename SourceStateDefinition, typename SuperStateDefinition>
const NoTransition<SourceStateDefinition, SuperStateDefinition> no_transition_react_result_;

template <typename SourceStateDefinition, typename SuperStateDefinition>
const NoReactionTransition<SourceStateDefinition, SuperStateDefinition> no_reaction_react_result_;

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

template <typename SourceStateDefinition, typename TargetStateDefinition>
struct IsValidTransition {
    using SuperStateDefinition = typename SourceStateDefinition::SSD;
    static constexpr bool value = IsInContext<TargetStateDefinition, SuperStateDefinition>::value;
};

template <typename StateDefinition, typename SuperStateDefinition>
class StateCrtp {
public:
    template <typename SubStateDefinition>
    using State = StateCrtp<SubStateDefinition, SuperStateDefinition>;
    using SSD = SuperStateDefinition;

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
    const ReactResult<StateDefinition, SuperStateDefinition>& transition() {
        static_assert(IsValidTransition<StateDefinition, TargetStateDefinition>::value);
        return regular_transition_react_result_<StateDefinition, TargetStateDefinition, SuperStateDefinition>;
    }

    void onEntry() { }
    void onExit() { }

private:
    decltype(auto) actual() {
        return static_cast<StateDefinition&>(*this);
    }

    decltype(auto) mixin() {
        return static_cast<StateMixin<StateDefinition, SuperStateDefinition>&>(*this);
    }
};

template <typename StateDefinitions, typename SuperStateDefinition>
class StateVariant;

template <typename ... StateDefinition, typename SuperStateDefinition>
struct StateVariant<std::tuple<StateDefinition...>, SuperStateDefinition> {
public:
    using Type = std::variant<StateMixin<StateDefinition, SuperStateDefinition>...>;
};

template <typename StateDefinition, typename SuperStateDefinition, typename HasSubstates = void> 
class SubState {
public:
    static inline constexpr bool defined = false;
};

template <typename StateDefinition, typename SuperStateDefinition>
class SubState<StateDefinition, SuperStateDefinition, typename StateDefinition::SubStates>
{
public:
    static inline constexpr bool defined = true;

    template <typename Event>
    decltype(auto) handleEvent(const Event& e) {
        return std::visit([](auto& active_sub_state){ return active_sub_state.handleEvent(); }, active_sub_state_variant_);
    }

    template <typename SourceStateDefinition>
    void executeTransition(const ReactResult<SourceStateDefinition, SuperStateDefinition>& react_result) {
        std::visit([](auto& active_sub_state){ active_sub_state.executeTransition(react_result); }, active_sub_state_variant_);
    }
private:
    typename StateVariant<typename StateDefinition::SubStates, SuperStateDefinition>::Type active_sub_state_variant_;
};

template <typename StateMachineDefinition>
class StateMachineMixin;

template <typename StateDefinition, typename SuperStateDefinition>
class StateMixin : public StateDefinition {
    using SubState = SubState<StateDefinition, SuperStateDefinition>;
    using SuperState = SuperState<StateDefinition, SuperStateDefinition>;
public:
    StateMixin(StateMixin<SuperState>& state_machine_mixin)
    : state_machine_mixin_{state_machine_mixin}
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

    template <typename TargetStateDefinition>
    void executeTransition() {
        if constexpr (SubState::defined) {
            active_sub_state_.template executeTransition<TargetStateDefinition>();
        }
        else {
            state_machine_mixin_.template executeTransition<StateDefinition, TargetStateDefinition>();
        }
    }

    template <typename ContextDefinition>
    void context() {
        if constexpr (SubState::defined) {
            active_sub_state_.template executeTransition<TargetStateDefinition>();
        }
        else {
            //using LCA = typename LCA<StateDefinition, TargetStateDefinition>::Type;
        }
    }

private:
    SubState active_sub_state_;
    StateMachineMixin<StateMachineDefinition>& state_machine_mixin_;
};

}