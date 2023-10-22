#pragma once

#include <tuple>
#include <variant>

namespace metahsm {


template <typename _T>
struct as_tuple {
    using type = std::tuple<_T>;
};

template <typename ... _T>
struct as_tuple<std::tuple<_T...>> {
    using type = std::tuple<_T...>;
};

template <typename _T>
using as_tuple_t = typename as_tuple<_T>::type;

template <typename ... _T>
using tuple_join_t = decltype(std::tuple_cat(std::declval<as_tuple_t<_T>>()...));

template <typename _Tuple>
struct reverse_tuple;

template <typename _First, typename ... _Rest>
struct reverse_tuple<std::tuple<_First, _Rest...>>
{
    using type = tuple_join_t<typename reverse_tuple<std::tuple<_Rest...>>::type, _First>;
};

template <typename _Last>
struct reverse_tuple<std::tuple<_Last>>
{
    using type = std::tuple<_Last>;
};

template <typename _Tuple>
using reverse_tuple_t = typename reverse_tuple<_Tuple>::type;

template <typename _T>
struct to_variant;

template <typename ... _T>
struct to_variant<std::tuple<_T...>>
{
    using type = std::variant<_T...>;
};

template <typename _T>
using to_variant_t = typename to_variant<_T>::type;

template <typename ... _T>
struct first_non_void
{
    using type = void;
};

template <typename ... _T>
struct first_non_void<void, _T...>
{
    using type = typename first_non_void<_T...>::type;
};

template <typename _T, typename ... _Us>
struct first_non_void<_T, _Us...>
{
    using type = _T;
};

template<typename ... T>
using first_non_void_t = typename first_non_void<T...>::type;

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

}