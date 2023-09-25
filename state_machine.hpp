#pragma once
#include "state.hpp"

namespace metahsm {

template <typename StateMachineDefinition>
class StateMachine : public StateCrtp<StateMachineDefinition, StateMachineDefinition> {

};

template <typename StateMachineDefinition>
class StateMachineMixin : public StateMachineDefinition {
    using SubState = SubState<StateMachineDefinition, StateMachineDefinition>;
    using Initial = typename Collapse<typename StateMachineDefinition::Initial, typename SubState::Default>::Type;
public:
    StateMachineMixin()
    : active_sub_state_{*this, StateSpec<Initial>{}}
    {}

    template <typename Event>
    bool dispatch(const Event& e) {
        if constexpr (SubState::defined) {
            decltype(auto) react_result = active_sub_state_.handleEvent(e);
            if(react_result.reaction_executed_) {
                return true;
            }
        }
        if constexpr(HasReactionToEvent<StateMachineDefinition, Event>::value) {
            decltype(auto) react_result = this->react(e);
            if(react_result.reaction_executed_) {
                react_result.executeTransition();
                return true;
            }
        }
        return false;
    }

    template <typename TargetState>
    void executeTransition() {
        if constexpr (SubState::defined) {
            active_sub_state_.template executeTransition<TargetState>();
        }
    }

    template <typename SourceState, typename TargetState>
    void executeTransition() {
        using LCA = metahsm::LCA<SourceState, TargetState>;
        if constexpr (SubState::defined) {
            active_sub_state_.template executeTransition<LCA, TargetStateDefinition>();
        }
    }

    template <typename ContextDefinition>
    void context() {
        if constexpr (SubState::defined) {
            //active_sub_state_.template executeTransition<TargetStateDefinition>();
        }
        else {
            //using LCA = typename LCA<StateDefinition, TargetStateDefinition>::Type;
        }
    }

private:
    SubState active_sub_state_;
};


}