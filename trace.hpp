// Copyright [2025] [Zoltán Rési]

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <tuple>
#include <string>
#include <iostream>

#include "type_traits.hpp"

namespace metahsm
{

template <typename T>
constexpr auto get_type_name(){
    std::string_view pretty_function = __PRETTY_FUNCTION__;
    auto start = pretty_function.find("T =") + 4;
    auto length = pretty_function.substr(start).find("]");
    return pretty_function.substr(start, length);
}

template <typename _TopStateDef>
const std::array<std::string_view, std::tuple_size_v<all_states_t<_TopStateDef>>> state_names = std::apply([](auto ... state) {
    return std::array{get_type_name<typename decltype(state)::type>()...};
}, tuple_apply_t<type_identity, all_states_t<_TopStateDef>>{});

template <typename Event_>
void trace_event() {
    std::cout << get_type_name<Event_>() << std::endl;
}

template <typename State_>
void trace_react(bool result, state_combination_t<top_state_t<State_>> & target) {
    std::string did_react = result ? "true" : "false";
    std::cout << "   " << get_type_name<State_>() << "::react: "  << did_react;
    if(target) {
        std::cout << ", target: {";
        bool first = true;
        for(std::size_t state_id = 0; state_id < std::tuple_size_v<all_states_t<top_state_t<State_>>>; state_id++) {
            if(target & (std::size_t{1} << state_id)) {
                if (first) { first = false; }
                else { std::cout << ","; }
                std::cout << state_names<top_state_t<State_>>.at(state_id);
            }
        }
        std::cout << "}";
    }
    else {
        std::cout << ", no target";
    }
    std::cout << std::endl;
}

template <typename _StateDef>
void trace_enter() {
    std::cout << "   " << get_type_name<_StateDef>() << "::enter"  << std::endl;
}

template <typename _StateDef>
void trace_exit() {
    std::cout << "   " << get_type_name<_StateDef>() << "::exit"  << std::endl;
}



}
