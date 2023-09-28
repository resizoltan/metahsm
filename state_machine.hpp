#pragma once
#include "state.hpp"

namespace metahsm {

class StateMachineBase
{};

template <typename TopStateDefinition>
class StateMachine
{
public:
    StateMachine()
    : top_state_(*this)
    {}

    template <typename Event>
    bool dispatch(const Event& event) {
        return top_state_.handleEvent(event);
    }

private:
    TopStateMixin<TopStateDefinition> top_state_;
};

}