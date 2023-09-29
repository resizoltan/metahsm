#include "metahsm.hpp"
#include <iostream>
#include <cassert>

using namespace metahsm;

class Event1 {};

struct MyTopState : public TopState<MyTopState> {
    struct Region0 : public State<Region0> {
        int i;

        auto react(Event1 event) {
            std::cout << "state0" << std::endl;
            return transition<State2>();
        }

        struct State1 : public State<State1> {
            int i = 0;
            auto react(Event1 event) {
                context<State1>();
                i = 1;
                if(i==1) return condition_not_met();
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
    struct Region1 : public State<Region1> {
        int i;

        auto react(Event1 event) {
            std::cout << "state0" << std::endl;
            return no_transition();
        }

        struct State3 : public State<State3> {
            int i = 0;
            auto react(Event1 event) {
                i = 1;
                if(i==1) return condition_not_met();
                std::cout << "state1" << std::endl;
                return no_transition();
            }
        };
        using SubStates = std::tuple<State3>;
    };
    using SubStates = std::tuple<Region0, Region1>;
};
StateMachine<MyTopState> state_machine;
int main(int argc, char *argv[]) {
    std::cout << "hi" << std::endl;
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
}