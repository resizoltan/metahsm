#include <iostream>
#include <cassert>
#include "metahsm2.hpp"



using namespace metahsm;
enum LifecycleEvent
{
  CONFIGURE,
  CLEANUP,
  ACTIVATE,
  DEACTIVATE
};

template <auto e>
struct Event{};

struct LifecycleTopState : State<LifecycleTopState>
{
  struct Unconfigured : State
  {
    auto react(Event<CONFIGURE>);
  };

  struct Inactive : State
  {
    auto react(Event<ACTIVATE>);
  };

  struct Active : State
  {
    auto react(Event<DEACTIVATE>);
    struct Monitoring : State
    {

    };
    
    using SubStates = std::tuple<Monitoring>;
  };

  using SubStates = std::tuple<Unconfigured, Inactive, Active>;
};

auto LifecycleTopState::Inactive::react(Event<ACTIVATE>) {
  transition<Active::Monitoring>();
}



template <typename T1, typename T2>
void ass() {
    static_assert(std::is_same_v<T1,T2>);
}

template <auto ... I>
void init(std::index_sequence<I...>) { 
  tuple_apply_t<StateMixin, all_states_t<LifecycleTopState>> states{(I, 1)...};
}

int main(int , char *[]) {
 StateMachine<LifecycleTopState> sm;
 init(std::make_index_sequence<std::tuple_size_v<all_states_t<LifecycleTopState>>>());
}