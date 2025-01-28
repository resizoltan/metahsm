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
    void react(Event<CONFIGURE>); // -> Inactive
  };

  struct Inactive : State
  {
    void react(Event<ACTIVATE>); // -> Active
  };

  struct Active : State
  {
    inline void react(Event<DEACTIVATE>) {
      transition<Inactive>();
    }
    int i = 0;

    struct Operation : State
    {
      struct Monitoring : State
      {
        inline void react(Event<ACTIVATE>) { transition<Commanding>(&action); }
        inline void action() { std::cout << "Action!" << std::endl; }
      };

      struct Commanding : State
      {
        inline void react(Event<CLEANUP>) { 
          transition<Monitoring>([this](){ std::cout << "Action1!" << std::endl; });
          transition<Safety::Error>([this](){ std::cout << "Action2!" << std::endl; });
        }
      };
      using SubStates = std::tuple<Monitoring, Commanding>;
    };

    struct Safety : State
    {
      struct Ok : State
      { };

      struct Error : State
      { };

      using SubStates = std::tuple<Ok, Error>;
    };

    using Regions = std::tuple<Operation, Safety>;
    
  };

  using SubStates = std::tuple<Unconfigured, Inactive, Active>;
};

void LifecycleTopState::Unconfigured::react(Event<CONFIGURE>) {
  transition<Inactive>([this](){ std::cout << "Action!" << std::endl; });
  auto& active_context = context<Active>();
  active_context.i = 1;
}

void LifecycleTopState::Inactive::react(Event<ACTIVATE>) {
  transition<Active>();
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
  sm.dispatch<Event<ACTIVATE>>();
  sm.dispatch<Event<CLEANUP>>();
  sm.dispatch<Event<DEACTIVATE>>();
}