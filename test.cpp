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

#define S(NAME) struct NAME : State
#define R(NAME) struct NAME : Region
#define states(...) using SubStates = std::tuple<__VA_ARGS__>;
#define regions(...) using Regions = std::tuple<__VA_ARGS__>;
#define on(EVENT) auto react(Event<EVENT>)
#define to(TARGET) { transition<TARGET>(); }

struct LifecycleTopState : State<LifecycleTopState> {
  S(Unconfigured)   { on(CONFIGURE)   to(Active)            };
  S(Inactive)       { on(ACTIVATE)    to(History<Active>)   };
  S(Active)         { on(DEACTIVATE)  to(Inactive)
    R(Operation)    {
      S(Monitoring)   { on(ACTIVATE)    to(Commanding)     };
      S(Commanding)   { on(CLEANUP)     to(Monitoring)     };  states(Monitoring, Commanding)
    };
    R(Safety)       {
      S(Ok)           { on(CLEANUP)     { }                 };
      S(Error)        {                                     };  states(Ok, Error)
    };                                                          regions(Operation, Safety)
  };                                                            states(Unconfigured, Inactive, Active)
};

struct LifecycleTopState2 : State<LifecycleTopState2>
{
  struct Unconfigured : State
  {
    void react(Event<CONFIGURE>) /*Inactive*/ {};
  };
  struct Inactive : State
  {
    void react(Event<ACTIVATE>) /*Active*/ {};
  };
  struct Active : State
  {
    inline void react(Event<DEACTIVATE>) /*Inactive*/ {
      transition<Inactive>();
    }
    int i = 0;
    struct Operation : Region
    {
      struct Monitoring : State
      {
        inline void react(Event<ACTIVATE>) /*Commanding*/ {
          transition<Commanding>();
          transition_action(&Monitoring::action);
        }
        inline void action() { std::cout << "Action!" << std::endl; }
      };
      struct Commanding : State
      {
        inline void react(Event<CLEANUP>) /*Monitoring, Error*/ { 
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
        inline void react(Event<CLEANUP>) /**/ {
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

struct LifecycleTopState3 : State<LifecycleTopState3>
{
  struct Unconfigured : State
  {
    void react(Event<CONFIGURE>); /*Inactive*/
  };
  struct Inactive : State
  {
    void react(Event<ACTIVATE>); /*Active*/
  };
  struct Active : State
  {
    inline void react(Event<DEACTIVATE>); /*Inactive*/
    int i = 0;
    struct Operation : Region
    {
      struct Monitoring : State
      {
        inline void react(Event<ACTIVATE>); /*Commanding*/
        inline void action();
      };
      struct Commanding : State
      {
        inline void react(Event<CLEANUP>); /*Monitoring, Error*/
      };
      using SubStates = std::tuple<Monitoring, Commanding>;
    };
    struct Safety : Region
    {
      struct Ok : State
      {
        inline void react(Event<CLEANUP>); /**/
      };
      struct Error : State
      { };
      using SubStates = std::tuple<Ok, Error>;
    };
    using Regions = std::tuple<Operation, Safety>;
  };
  using SubStates = std::tuple<Unconfigured, Inactive, Active>;
};

struct LifecycleTopState3 : State<LifecycleTopState3>
{
  S(Unconfigured) {   on(CONFIGURE);  /*Inactive*/          };
  S(Inactive)     {   on(ACTIVATE);   /*Active*/            };
  S(Active)       {   on(DEACTIVATE); /*Inactive*/
    int i = 0;
    R(Operation) {
      S(Monitoring) {   on(ACTIVATE); /*Commanding*/        };
      S(Commanding) {   on(CLEANUP); /*Monitoring, Error*/  };
      states(Monitoring, Commanding)                        };
    R(Safety) {
      S(Ok)         {   on(CLEANUP); /**/                   };
      S(Error)      {                                       };
      states(Ok, Error)                                     };
    regions(Operation, Safety)                              };
  states(Unconfigured, Inactive, Active)
};




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