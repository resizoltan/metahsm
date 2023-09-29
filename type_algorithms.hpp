#pragma once

namespace metahsm {

template <typename ... _Tuple>
using tuple_join_t = decltype(std::tuple_cat(std::declval<_Tuple>()...));

template <typename _T>
struct to_variant;

template <typename ... _T>
struct to_variant<std::tuple<_T...>>
{
    using type = std::variant<_T...>;
};

template <typename _T>
using to_variant_t = typename to_variant<_T>::type;

template <typename ... T>
struct collapse
{
    using type = void;
};

template <typename ... T>
struct collapse<void, T...>
{
    using type = typename collapse<T...>::type;
};

template <typename T, typename ... Us>
struct collapse<T, Us...>
{
    using type = T;
};

template<typename T>
using collapse_t = typename collapse<T>::type;


}