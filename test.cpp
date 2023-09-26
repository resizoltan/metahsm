#include "state.hpp"
#include "state_machine.hpp"
#include <iostream>
#include <cassert>

using namespace metahsm;

class Event1 {};

struct StateMachine0 : public StateMachine<StateMachine0> {
    struct State0 : public State<State0> {
        int i;
        struct State1 : public State<State1> {
            int i = 0;
            auto react(Event1 event) {
                context<State1>().i = 0;
                i = 1;
                std::cout << "state1" << std::endl;
                return transition<State2>();
            }
        };

        struct State2 : public State<State2> {
            auto react(Event1 event) {
                //context<State1>().i = 0;
                std::cout << "state2" << std::endl;
                return transition<State1>();
            }
        };
        using SubStates = std::tuple<State1, State2>;
    };
    using SubStates = std::tuple<State0>;
};

StateMachineMixin<StateMachine0> state_machine;
static_assert(std::is_same_v<StateMachine0::State0, LCA<StateMachine0::State0::State1, StateMachine0::State0::State2>>);
int main(int argc, char *argv[]) {
    std::cout << "hi" << std::endl;
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
}