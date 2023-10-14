#include "metahsm.hpp"
#include <iostream>
#include <cassert>

using namespace metahsm;

class Event1 {};

struct MyTopState : public TopState<MyTopState> {

    auto react(Event1 event) {
        std::cout << "mytopstate" << std::endl;
        transition<Region0::State2>();
        return true;
    }

    void on_entry() {
        std::cout << "entry: mytopstate" << std::endl;
    }

    void on_exit() {
        std::cout << "exit: mytopstate" << std::endl;
    }

    struct Region0 : public State<Region0> {
        int i;

        void on_entry() {
            std::cout << "entry: region0" << std::endl;
        }

        void on_exit() {
            std::cout << "exit: region0" << std::endl;
        }

        struct State1 : public State<State1> {
            int i = 0;

            void on_entry() {
                std::cout << "entry: state1" << std::endl;
            }

            void on_exit() {
                std::cout << "exit: state1" << std::endl;
            }
            auto react(Event1 event) {
                context<State1>();
                i = 1;
                //if(i==1) return condition_not_met();
                std::cout << "state1" << std::endl;
                transition<State2>();
                return true;
            }
        };

        struct State2 : public State<State2> {

            void on_entry() {
                std::cout << "entry: state2" << std::endl;
            }

            void on_exit() {
                std::cout << "exit: state2" << std::endl;
            }
            auto react(Event1 event) {
                //context<State1>().i = 0;
                std::cout << "state2" << std::endl;
                transition<Region1>();
                return true;
            }
        };
        using SubStates = std::tuple<State1, State2>;
    };
    struct Region1 : public State<Region1> {
        int i;


        void on_entry() {
            std::cout << "entry: region1" << std::endl;
        }

        void on_exit() {
            std::cout << "exit: region1" << std::endl;
        }
        auto react(Event1 event) {
            std::cout << "region1" << std::endl;
            return true;
        }

        struct State3 : public State<State3> {
            int i = 0;

            void on_entry() {
                std::cout << "entry: state3" << std::endl;
            }

            void on_exit() {
                std::cout << "exit: state3" << std::endl;
            }
            auto react(Event1 event) {
                i = 1;
                if(i==1) return false;
                std::cout << "state3" << std::endl;
                return true;
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