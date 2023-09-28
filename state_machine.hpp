#pragma once
#include "state.hpp"

namespace metahsm {

class StateMachineBase
{};

template <typename _TopStateDef>
class StateMachine
{
public:
    StateMachine()
    : top_state_(*this)
    {}

    template <typename _Event>
    bool dispatch(const _Event& event) {
        return top_state_.handleEvent(event);
    }

private:
    TopStateMixin<_TopStateDef> top_state_;
};

}