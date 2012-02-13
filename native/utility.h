#ifndef __UTILITY_H__
#define __UTILITY_H__

#include "base.h"
#include <tuple>

namespace dev
{
    namespace util
    {
        // Merges two std::tuple<>s.
        template<typename...>
        struct tuple_merge;

        template<typename ...TA, typename ...TB>
        struct tuple_merge<std::tuple<TA...>, std::tuple<TB...>>
        { typedef std::tuple<TA..., TB...> type; };

        template<typename ...TA>
        struct tuple_merge<std::tuple<TA...>>
        { typedef std::tuple<TA...> type; };

        template<>
        struct tuple_merge<>
        { typedef std::tuple<> type; };

        // Appends an element to std::tuple<>.
        template<typename, typename >
        struct tuple_append;

        template<typename A, typename ...T>
        struct tuple_append<A, std::tuple<T...> >
        { typedef std::tuple<A, T...> type; };

        // Prepends an element to std::tuple<>.
        template<typename, typename >
        struct tuple_prepend;

        template<typename ...T, typename A>
        struct tuple_prepend<std::tuple<T...>, A>
        { typedef std::tuple<T..., A> type; };

        // make a new tuple from elements at even index
        template<typename>
        struct tuple_even_elements;

        template<typename A, typename B>
        struct tuple_even_elements<std::tuple<A, B>>
        { typedef std::tuple<A> type; };

        template<typename A, typename B, typename ...T>
        struct tuple_even_elements<std::tuple<A, B, T...>>
        { typedef typename tuple_append<A, typename tuple_even_elements<std::tuple<T...>>::type>::type type; };

        template<>
        struct tuple_even_elements<std::tuple<>>
        { typedef std::tuple<> type; };

        // make a new tuple from elements at odd index
        template<typename>
        struct tuple_odd_elements;

        template<typename A, typename B>
        struct tuple_odd_elements<std::tuple<A, B>>
        { typedef std::tuple<B> type; };

        template<typename A, typename B, typename ...T>
        struct tuple_odd_elements<std::tuple<A, B, T...>>
        { typedef typename tuple_append<B, typename tuple_odd_elements<std::tuple<T...>>::type>::type type; };

        template<>
        struct tuple_odd_elements<std::tuple<>>
        { typedef std::tuple<> type; };


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
