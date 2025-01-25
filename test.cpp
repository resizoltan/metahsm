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
    void react(Event<CONFIGURE>);
  };

  struct Inactive : State
  {
    void react(Event<ACTIVATE>);
  };

  struct Active : State
  {
    void react(Event<DEACTIVATE>);
    struct Monitoring : State
    {

    };
    
    using SubStates = std::tuple<Monitoring>;
  };

  using SubStates = std::tuple<Unconfigured, Inactive, Active>;
};

void LifecycleTopState::Unconfigured::react(Event<CONFIGURE>) {
  transition<Inactive>();
}

void LifecycleTopState::Inactive::react(Event<ACTIVATE>) {
  transition<Active::Monitoring>();
}

void LifecycleTopState::Active::react(Event<DEACTIVATE>) {
  transition<Inactive>();
}



template <typename T1, typename T2>
void ass() {
    static_assert(std::is_same_v<T1,T2>);
}

template <auto ... I>
void init(std::index_sequence<I...>) {
   StateMachine<LifecycleTopState> sm;

  //tuple_apply_t<StateMixin, all_states_t<LifecycleTopState>> states{(I, 1)...};
}

int main(int , char *[]) {
  //static_assert(std::is_invocable_v<decltype(&LifecycleTopState::Unconfigured::react), LifecycleTopState::Unconfigured&, const Event<CONFIGURE>&>);
  StateMachine<LifecycleTopState> sm;
  sm.dispatch<Event<CONFIGURE>>();
  sm.dispatch<Event<ACTIVATE>>();
  sm.dispatch<Event<DEACTIVATE>>();
}