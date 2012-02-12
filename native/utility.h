#ifndef __UTILITY_H__
#define __UTILITY_H__

#include "base.h"
#include <tuple>

namespace dev
{
    namespace utility
    {
        // append to tuple
        template<typename, typename >
        struct tuple_append;

        template<typename H, typename ...T>
        struct tuple_append<H, std::tuple<T...> >
        { typedef std::tuple<H, T...> type; };

        // make a new tuple from elements at even index
        template<typename ...>
        struct tuple_even_elements;

        template<typename A, typename B>
        struct tuple_even_elements<A, B>
        { typedef std::tuple<A> type; };

        template<typename A, typename B, typename ...T>
        struct tuple_even_elements<A, B, T...>
        { typedef typename tuple_append<A, typename tuple_even_elements<T...>::type>::type type; };

        // make a new tuple from elements at odd index
        template<typename ...>
        struct tuple_odd_elements;

        template<typename A, typename B>
        struct tuple_odd_elements<A, B>
        { typedef std::tuple<B> type; };

        template<typename A, typename B, typename ...T>
        struct tuple_odd_elements<A, B, T...>
        { typedef typename tuple_append<B, typename tuple_odd_elements<T...>::type>::type type; };



        // get the first index of a type in the tuple
        template<typename T, typename C, std::size_t I>
        struct tuple_index_r;

        template<typename H, typename ...R, typename C, std::size_t I>
        struct tuple_index_r<std::tuple<H, R...>, C, I>
            : public std::conditional<std::is_same<C, H>::value,
                  std::integral_constant<std::size_t, I>,
                  tuple_index_r<std::tuple<R...>, C, I+1>>::type
        {};

        template<typename C, std::size_t I>
        struct tuple_index_r<std::tuple<>, C, I>
        {};

        template<typename T, typename C>
        struct tuple_index_of
            : public std::integral_constant<std::size_t, tuple_index_r<T, C, 0>::value> {};
    }
}

#endif
