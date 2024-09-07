#pragma once

#include <tuple>
#include <string>
#include <iostream>

#include "type_traits.hpp"

namespace metahsm
{

template <typename T>
constexpr auto get_type_name(){
    std::string pretty_function = __PRETTY_FUNCTION__;
    auto start = pretty_function.find("T =") + 3;
    auto length = pretty_function.substr(start).find("]");
    return pretty_function.substr(start, length);
}

template <typename _TopStateDef>
const std::array<std::string, std::tuple_size_v<all_states_t<_TopStateDef>>> state_names = std::apply([](auto ... state) {
    return std::array{get_type_name<typename decltype(state)::type>()...};
}, tuple_apply_t<type_identity, all_states_t<_TopStateDef>>{});

template <typename _StateDef>
void trace_react(ReactionResult<typename _StateDef::TopStateDef> result) {
    std::string did_react = result.reacted ? "true" : "false";
    std::size_t target = result.target_combination;
    std::cout << get_type_name<_StateDef>() << "::react: "  << did_react;
    if(target != 0) {
        std::cout << ", target: ";
        for(std::size_t state_id = 0; state_id < std::tuple_size_v<all_states_t<typename _StateDef::TopStateDef>>; state_id++) {
            if(static_cast<bool>((1 << state_id) & target)) {
                std::cout << state_names<typename _StateDef::TopStateDef>.at(state_id) << ", ";
            }
        }
    }
    else {
        std::cout << ", no target";
    }
    std::cout << std::endl;
}

template <typename _StateDef>
void trace_enter() {
    std::cout << get_type_name<_StateDef>() << "::enter"  << std::endl;
}

template <typename _StateDef>
void trace_exit() {
    std::cout << get_type_name<_StateDef>() << "::exit"  << std::endl;
}



}
