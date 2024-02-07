#pragma once

#include <tuple>
#include <variant>

namespace metahsm {

template <typename T>
struct type_identity{ using type = T; };

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

template <typename Tuple>
struct as_ref_tuple;

template <typename ... T>
struct as_ref_tuple<std::tuple<T...>>
{
    using type = std::tuple<T&...>;
};

template <typename T>
using as_ref_tuple_t = typename as_ref_tuple<T>::type;

template <typename _T>
struct to_variant;

template <typename ... _T>
struct to_variant<std::tuple<_T...>>
{
    using type = std::variant<_T...>;
};

template <typename _T>
using to_variant_t = typename to_variant<_T>::type;

template <typename _Default, typename ... _T>
struct first_non_void
{
    using type = _Default;
};

template <typename _Default, typename ... _T>
struct first_non_void<_Default, void, _T...>
{
    using type = typename first_non_void<_Default, _T...>::type;
};

template <typename _Default, typename _T, typename ... _Us>
struct first_non_void<_Default, _T, _Us...>
{
    using type = _T;
};

template<typename _Default, typename ... T>
using first_non_void_t = typename first_non_void<_Default, T...>::type;

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


}