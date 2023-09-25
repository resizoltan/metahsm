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
    static inline constexpr bool value = std::is_invocable_r_v<decltype(&StateDefinition::react), Event, Target>;
};

template <typename StateDefinition>
class StateMixin;

template<typename SourceStateDefinition>
class ReactResult
{
public:
    ReactResult(bool reaction_executed)
    : reaction_executed_{reaction_executed}
    {}

    virtual void executeTransition(StateMixin<SourceStateDefinition>& source) = 0;
    bool reaction_executed_;
};

template <typename SourceStateDefinition, typename TargetStateDefinition>
class RegularTransition : public ReactResult<SourceStateDefinition> {
public:
    RegularTransition()
    : ReactResult(true)
    {}

    void executeTransition(StateMixin<SourceStateDefinition>& source) override {
        source.template executeTransition<TargetStateDefinition>();
    }
};

template <typename SourceStateDefinition>
class NoTransition : public ReactResult<SourceStateDefinition>
{
public:
    NoTransition()
    : ReactResult(true)
    {}

    void executeTransition(StateMixin<SourceStateDefinition>& source) override {
        (void)source;
    }
};

template <typename SourceStateDefinition>
class NoReactionTransition : public ReactResult<SourceStateDefinition>
{
public:
    NoReactionTransition()
    : ReactResult(false)
    {}

    void executeTransition(StateMixin<SourceStateDefinition>& source) override {
        (void)source;
    }
};

template <typename SourceStateDefinition, typename TargetStateDefinition>
const RegularTransition<SourceStateDefinition, TargetStateDefinition> regular_transition_react_result_;

template <typename SourceStateDefinition>
const NoTransition<SourceStateDefinition> no_transition_react_result_;

template <typename SourceStateDefinition>
const NoReactionTransition<SourceStateDefinition> no_reaction_react_result_;

template <typename StateDefinition, typename StateMachineDefinition>
class StateCrtp {
public:
    template <typename SubStateDefinition>
    using State = StateCrtp<SubStateDefinition, StateMachineDefinition>; 

    StateCrtp() {
        onEntry();
    }

    ~StateCrtp() {
        onExit();
    }

    void init(StateMixin<StateDefinition> *state_mixin){
        state_mixin_ = state_mixin;
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
    const ReactResult<StateDefinition>& transition() {
        return regular_transition_react_result_<StateDefinition, TargetStateDefinition>;
    }

    void onEntry() { }
    void onExit() { }

private:
    decltype(auto) actual() {
        return static_cast<StateDefinition&>(*this);
    }

    decltype(auto) mixin() {
        return static_cast<StateMixin<StateDefinition>&>(*this);
    }
};

template <typename StateDefinitions>
class StateVariant;

template <typename ... StateDefinition>
struct StateVariant<std::tuple<StateDefinition...>> {
public:
    using Type = std::variant<StateMixin<StateDefinition>...>;
};

template <typename StateDefinition, typename HasSubstates = void> 
class SubState {
public:
    static inline constexpr bool defined = false;
};

template <typename StateDefinition>
class SubState<StateDefinition, typename StateDefinition::SubStates>
{
public:
    static inline constexpr bool defined = true;

    template <typename Event>
    decltype(auto) handleEvent(const Event& e) {
        return std::visit([](auto& active_sub_state){ return active_sub_state.handleEvent(); }, active_sub_state_variant_);
    }

    template <typename SourceStateDefinition>
    void executeTransition(const ReactResult<SourceStateDefinition>& react_result) {
        std::visit([](auto& active_sub_state){ active_sub_state.executeTransition(react_result); }, active_sub_state_variant_);
    }
private:
    typename StateVariant<typename StateDefinition::SubStates>::Type active_sub_state_variant_;
};

template <typename StateDefinition>
class StateMixin : public StateDefinition {
    using SubState = SubState<StateDefinition>;
public:
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
                react_result.executeTransition();
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
           // using LCA = typename LCA<StateDefinition, TargetStateDefinition>::Type;
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
};

}