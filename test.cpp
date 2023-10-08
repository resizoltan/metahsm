#include "metahsm.hpp"
#include <iostream>
#include <cassert>

using namespace metahsm;

class Event1 {};

struct MyTopState : public TopState<MyTopState> {

    auto react(Event1 event) {
        std::cout << "mytopstate" << std::endl;
        return transition<Region0::State2>();
    }
    struct Region0 : public State<Region0> {
        int i;

        struct State1 : public State<State1> {
            int i = 0;
            auto react(Event1 event) {
                context<State1>();
                i = 1;
                //if(i==1) return condition_not_met();
                std::cout << "state1" << std::endl;
                return transition<State2>();
            }
        };

        struct State2 : public State<State2> {
            auto react(Event1 event) {
                //context<State1>().i = 0;
                std::cout << "state2" << std::endl;
                return transition<Region1>();
            }
        };
        using SubStates = std::tuple<State1, State2>;
    };
    struct Region1 : public State<Region1> {
        int i;

        auto react(Event1 event) {
            std::cout << "region1" << std::endl;
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
template <typename T1, typename T2>
struct assert_same {
    static_assert(std::is_same_v<T1, T2>);
};
static_assert(is_top_state_v<typename decltype(MyTopState::Region0::top_state_spec())::type>);
assert_same<super_state_t<MyTopState::Region0>, MyTopState> ass;
TopStateMixin<MyTopState> ts{state_machine};
int main(int argc, char *argv[]) {
    std::cout << "hi" << std::endl;
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
    state_machine.dispatch(Event1{}); 
}