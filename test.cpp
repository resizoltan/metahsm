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
    struct Operation : Region
    {
      struct Monitoring : State
      {
        inline void react(Event<ACTIVATE>) {
          transition<Commanding>();
          transition_action(&Monitoring::action);
        }
        inline void action() { std::cout << "Action!" << std::endl; }
      };
      struct Commanding : State
      {
        inline void react(Event<CLEANUP>) { 
          std::cout << "Before Action1!" << std::endl;
          transition<Monitoring>();
          transition<Safety::Error>();
          transition_action([this](){ std::cout << "Action1!" << std::endl; });
        }
      };
      using SubStates = std::tuple<Monitoring, Commanding>;
    };
    struct Safety : Region
    {
      struct Ok : State
      {
        inline void react(Event<CLEANUP>) {
          std::cout << "Before Action3!" << std::endl;
          transition_action([this](){ std::cout << "Action3!" << std::endl; });
        }
      };
      struct Error : State
      { };
      using SubStates = std::tuple<Ok, Error>;
    };
    using Regions = std::tuple<Operation, Safety>;
  };
  using SubStates = std::tuple<Unconfigured, Inactive, Active>;
};

void LifecycleTopState::Unconfigured::react(Event<CONFIGURE>) {
  transition<Inactive>();
  auto& active_context = context<Active>();
  active_context.i = 1;
}

void LifecycleTopState::Inactive::react(Event<ACTIVATE>) {
  transition<History<Active>>();
  assert(is_in_state<Inactive>());
}

template <typename Config>
struct TLC : StateTemplate<TLC>, Config
{
  struct Unconfigured : State, Config
  {
    inline void react(Event<CONFIGURE>) {
      transition<Inactive>();
    }
  };
  using Inactive = typename Config::Inactive;
  using SubStates = std::tuple<Unconfigured, Inactive>;
};

struct Inactive1;
struct TLCConfig1 : Config<TLCConfig1>
{
  using Inactive = Inactive1;
};
struct Inactive1 : State<TLC<TLCConfig1>>
{
  inline void react(Event<ACTIVATE>) { };
};

struct Inactive2;
struct TLC2TopState;
struct TLCConfig2 : Config<TLCConfig2>
{
  using Inactive = Inactive2;
  using TopState = TLC2TopState;
};

struct TLC2TopState : State<TLC2TopState>
{
  using SubStates = std::tuple<TLC<TLCConfig2>>;
};

struct Inactive2 : State<TLC2TopState>
{
  inline void react(Event<ACTIVATE>) { };
};


template <typename Config>
struct TLC3 : StateTemplate<TLC3>, Config
{
  struct Unconfigured : State, Config
  {
    inline void react(Event<CONFIGURE>) {
      transition<Unconfigured>();
    }
  };
  using SubStates = std::tuple<Unconfigured>;
};
struct TLC3TopState : State<TLC3TopState>
{
  using Regions = std::tuple<TLC3<TopStateRebind<TLC3TopState>>>;
};

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
  sm.dispatch<Event<ACTIVATE>>();
 /*static_assert(!std::is_void_v<TLC<TLCConfig2>::Conf::TopState>);
  ass<typename SimpleStateWrapper<TLC<TLCConfig2>::Unconfigured>::TopState, TLC2TopState>();

  StateMachine<TLC<TLCConfig1>> smt1;
  smt1.dispatch<Event<CONFIGURE>>();
  smt1.dispatch<Event<ACTIVATE>>();


  StateMachine<TLC2TopState> smt2;
  smt2.dispatch<Event<CONFIGURE>>();
  smt2.dispatch<Event<ACTIVATE>>();

  StateMachine<TLC3TopState> smt3;
  smt3.dispatch<Event<CONFIGURE>>();
  smt3.dispatch<Event<ACTIVATE>>();*/
}