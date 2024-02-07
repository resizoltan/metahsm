#include "metahsm.hpp"
#include <iostream>
#include <cassert>

using namespace metahsm;

class Event1 {};

struct MyTopState : public TopState<MyTopState> {

    auto react(Event1 event) {
        transition<Region0::State2>();
        return true;
    }

    struct Region0 : public State<Region0> {
        int i;

        struct State1 : public State<State1> {
            int i = 0;
            auto react(Event1 event) {
                //context<Region1::State3>().i = 1;
                transition<State2>();
                return true;
            }
        };

        struct State2 : public State<State2> {
            auto react(Event1 event) {
                //context<State1>().i = 0;
                transition<State1>();
                return true;
            }
        };
        using SubStates = std::tuple<State1, State2>;
        using Initial = State2;
    };
    struct Region1 : public State<Region1> {
        int i;

        auto react(Event1 event) {
            transition<Region0::State2>();
            return true;
        }

        struct State3 : public State<State3> {
            int i = 0;
            auto react(Event1 event) {
                if(i==1) return false;
                return true;
            }
        };
        using SubStates = std::tuple<State3>;
    };
    using Regions = std::tuple<Region0, Region1>;
};

template <typename T1, typename T2>
void ass() {
    static_assert(std::is_same_v<T1,T2>);
}
StateMachine<MyTopState> state_machine;
int main(int argc, char *argv[]) {
    ass<super_state_t<MyTopState>, RootState>();
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
}