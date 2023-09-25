#include "state.hpp"
#include "state_machine.hpp"
#include <iostream>

using namespace metahsm;

class Event1 {};

class StateMachine0 : public StateMachine<StateMachine0> {
    class State0 : public State<State0> {
    public:
        int i;
        class State1 : public State<State1> {
            int i = 0;
            auto react(Event1 event) {
                context<State1>().i = 0;
                i = 1;
                return transition<State2>();
            }
        };

        class State2 : public State<State2> {

        };
        using SubStates = std::tuple<State1, State2>;
    };
};

StateMachineMixin<StateMachine0> state_machine;

int main(int argc, char *argv[]) {
    state_machine.dispatch(Event1{});    
    //auto t = state0.template transition<State0::State2>();
}