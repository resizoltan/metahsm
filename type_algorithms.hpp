// Copyright 2025 Zoltán Rési

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
#include <variant>

namespace metahsm {

template <typename T>
struct type_identity{ using type = T; };

template <typename T>
struct type_identity_tuple_
{};

template <typename ... T>
struct type_identity_tuple_<std::tuple<T...>>
{
    using type = std::tuple<type_identity<T>...>;
};

template <typename T>
using type_identity_tuple = typename type_identity_tuple_<T>::type;

template <typename>
struct is_tuple: std::false_type {};

template <typename ... T>
struct is_tuple<std::tuple<T...>>: std::true_type {};

template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename _T>
struct as_tuple { using type = std::tuple<_T>; };

template <typename ... _T>
struct as_tuple<std::tuple<_T...>> { using type = std::tuple<_T...>; };

template <typename _T>
using as_tuple_t = typename as_tuple<_T>::type;

template <typename ... _T>
using tuple_join_t = decltype(std::tuple_cat(std::declval<as_tuple_t<_T>>()...));

template <typename _T>
struct to_variant;

template <typename ... _T>
struct to_variant<std::tuple<_T...>>
{
    using type = std::variant<_T...>;
};

template <typename _T>
using to_variant_t = typename to_variant<_T>::type;

template <typename _T, typename _Tuple>
struct index;

template <typename _T, typename ... _Us>
struct index<_T, std::tuple<_T, _Us...>> {
    static constexpr std::size_t value = 0;
};

template <typename _T, typename _U, typename ... _Us>
struct index<_T, std::tuple<_U, _Us...>> {
    static constexpr std::size_t value = 1 + index<_T, std::tuple<_Us...>>::value;
};

template <typename _T, typename _Tuple>
constexpr std::size_t index_v = index<_T, _Tuple>::value;

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

template <template <typename> typename _F, typename _Tuple>
struct tuple_apply;

template <template <typename> typename _F, typename ... _T>
struct tuple_apply<_F, std::tuple<_T...>>
{
    using type = std::tuple<_F<_T>...>;
};

template <template <typename> typename _F, typename _Tuple>
using tuple_apply_t = typename tuple_apply<_F, _Tuple>::type;

template <typename _Tuple>
struct tuple_strip_void;

template <typename _Tuple>
using tuple_strip_void_t = typename tuple_strip_void<_Tuple>::type;

template <typename _T1, typename ... _T>
struct tuple_strip_void<std::tuple<_T1, _T...>>
{
    using rest = tuple_strip_void_t<std::tuple<_T...>>;
    using type = std::conditional_t<std::is_same_v<_T1, void>, rest, tuple_join_t<_T1, rest>>;
};

template <>
struct tuple_strip_void<std::tuple<>>
{
    using type = std::tuple<>;
};

template <template <typename> typename _F, typename _Tuple>
struct tuple_filter;

template <template <typename> typename _F, typename ... _T>
struct tuple_filter<_F, std::tuple<_T...>>
{
    using type = tuple_strip_void_t<std::tuple<std::conditional_t<_F<_T>::value, _T, void>...>>;
};

template <template <typename> typename _F, typename _Tuple>
using tuple_filter_t = typename tuple_filter<_F, _Tuple>::type;

std::size_t bit_index(std::size_t x) {
    std::size_t n = 64;
    if ( (x>>32) != 0 ) { n=n-32; x = x>>32; } 
    if ( (x>>16) != 0 ) { n=n-16; x = x>>16; } 
    if ( (x>>8 ) != 0 ) { n=n- 8; x = x>> 8; } 
    if ( (x>>4 ) != 0 ) { n=n- 4; x = x>> 4; } 
    if ( (x>>2 ) != 0 ) { n=n- 2; x = x>> 2; } 
    if ( (x>>1 ) != 0 ) { return 63-(n-2); } 
    return 63-(n - x); 
}

}