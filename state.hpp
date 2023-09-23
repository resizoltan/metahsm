#pragma once

#include <type_traits>
#include <tuple>
#include <variant>

namespace metahsm {

template <class StateDefinition>
struct Target {
    using SD = StateDefinition;
};

template <typename Event, typename StateDefinition, typename Target, typename Invocable = void>
struct IsTransitionDefined {
    static inline constexpr bool value = false;
};

template <typename Event, typename StateDefinition, typename Target>
struct IsTransitionDefined<Event, StateDefinition, Target, std::is_invocable_r<bool, decltype(&StateDefinition::transition), Event, Target>> {
    static inline constexpr bool value = std::is_invocable_r_v<bool, decltype(&StateDefinition::transition), Event, Target>;
    static inline constexpr bool has_condition = true;
};

template <typename Event, typename StateDefinition, typename Target>
struct IsTransitionDefined<Event, StateDefinition, Target, std::is_invocable_r<void, decltype(&StateDefinition::transition), Event, Target>> {
    static inline constexpr bool value = std::is_invocable_r_v<bool, decltype(&StateDefinition::transition), Event, Target>;
    static inline constexpr bool has_condition = false;
};

template <typename StateDefinitions>
class StateVariant;

template <typename ... StateDefinition>
struct StateVariant<std::tuple<StateDefinition...>> {
public:
    using Type = std::variant<State<StateDefinition>...>;
};

template <typename StateDefinition, typename HasSubstates = void> 
class SubState {
public:
    template <typename Event, typename TargetStateDefinition>
    bool handleEvent(const Event& e) {
        return false;
    }
};

template <typename StateDefinition>
class SubState<StateDefinition, typename StateDefinition::SubStates>
{
public:
    template <typename Event, typename TargetStateDefinition>
    bool handleEvent(const Event& e) {
        return std::visit([](auto& active_sub_state){ return active_sub_state.handleEvent(); }, active_sub_state_variant_);
    }
private:
    typename StateVariant<typename StateDefinition::SubStates>::Type active_sub_state_variant_;
};

template <typename StateDefinition>
class State {
public:
    template <typename Event, typename TargetStateDefinition>
    bool handleEvent(const Event& e) {
        bool event_handled = active_sub_state_.handleEvent(e);
        if(!event_handled) {
            static constexpr bool transition_defined = IsTransitionDefined<Event, StateDefinition, TargetStateDefinition>::value;
            if constexpr(IsTransitionDefined<Event, StateDefinition, TargetStateDefinition>::value) {
                if constexpr(IsTransitionDefined<Event, StateDefinition, TargetStateDefinition>::has_condition) {
                    event_handled = state_definition_.transition(e);
                }
                else {
                    state_definition_.transition(e);
                    event_handled = true;
                }
            }
        }
    }

private:
    StateDefinition state_definition_;
    SubState<StateDefinition> active_sub_state_;
};


}