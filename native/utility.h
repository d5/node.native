#ifndef __UTILITY_H__
#define __UTILITY_H__

#include "base.h"
#include <tuple>
#include <stdexcept>

namespace native
{
    namespace util
    {
        /**
         *  Merges all tuples into a single tuple.
         */
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

        /**
         *  Appends an element to a tuple.
         */
        template<typename, typename >
        struct tuple_append;

        template<typename A, typename ...T>
        struct tuple_append<A, std::tuple<T...> >
        { typedef std::tuple<A, T...> type; };

        /**
         *  Prepends an element to a tuple.
         */
        template<typename, typename >
        struct tuple_prepend;

        template<typename ...T, typename A>
        struct tuple_prepend<std::tuple<T...>, A>
        { typedef std::tuple<T..., A> type; };

        /**
         *  Creates a new tuple that consists of even elements of the input tuple.
         */
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

        /**
         *  Creates a new tuple that consists of odd elements of the input tuple.
         */
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

        // recursive helper for tuple_index_of<>.
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

        /**
         *  Gets the first index of type C in the tuple T.
         */
        template<typename T, typename C>
        struct tuple_index_of
            : public std::integral_constant<std::size_t, tuple_index_r<T, C, 0>::value> {};

        template<typename ...A>
        struct callback_def
        {
            typedef std::function<void(A...)> callback_type;
        };

        class dict
        {
            typedef std::map<std::string, std::string> map_type;
            typedef map_type::iterator iterator;
            typedef map_type::const_iterator const_iterator;
            typedef map_type::reverse_iterator reverse_iterator;
            typedef map_type::const_reverse_iterator const_reverse_iterator;

        public:
            dict()
                : map_()
            {}

            dict(const map_type& map)
                : map_(map)
            {}

            dict(const dict& c)
                : map_(c.map_)
            {}

            dict(dict&& c)
                : map_(std::move(c.map_))
            {}

            dict(std::initializer_list<map_type::value_type> map)
                : map_(map)
            {}

            dict(std::nullptr_t)
                : map_()
            {}

            virtual ~dict()
            {}

            dict& operator =(const dict& c)
            {
                map_ = c.map_;
                return *this;
            }

            dict& operator =(dict&& c)
            {
                map_ = std::move(c.map_);
                return *this;
            }

            dict& operator =(std::initializer_list<map_type::value_type> map)
            {
                map_ = map;
                return *this;
            }

            const std::string& get(const std::string& name, const std::string& default_value=std::string()) const
            {
                auto it = map_.find(name);
                if(it != map_.end()) return it->second;
                else return default_value;
            }

            bool get(const std::string& name, std::string& value) const
            {
                auto it = map_.find(name);
                if(it == map_.end()) return false;
                value = it->second;
                return true;
            }

            std::string& operator[](const std::string& name)
            {
                return map_[name];
            }

            const std::string& operator[](const std::string& name) const
            {
                auto it = map_.find(name);
                if(it == map_.end()) throw std::out_of_range("No such element exists.");
                return it->second;
            }

            bool compare(const std::string& name, const std::string& value) const
            {
                auto it = map_.find(name);
                if(it == map_.end()) return false;
                return value.compare(it->second) == 0;
            }

            // TODO: implement options::compare_no_case() function.
            bool compare_no_case(const std::string& name, const std::string& value) const;

            bool has(const std::string& name) const
            {
                return map_.count(name) > 0;
            }

            iterator begin() { return map_.begin(); }
            const_iterator begin() const { return map_.begin(); }
            iterator end() { return map_.end(); }
            const_iterator end() const { return map_.end(); }
            reverse_iterator rbegin() { return map_.rbegin(); }
            const_reverse_iterator rbegin() const { return map_.rbegin(); }
            reverse_iterator rend() { return map_.rend(); }
            const_reverse_iterator rend() const { return map_.rend(); }
            const_iterator cbegin() const { return map_.cbegin(); }
            const_iterator cend() const { return map_.cend(); }
            const_reverse_iterator crbegin() const { return map_.crbegin(); }
            const_reverse_iterator crend() const { return map_.crend(); }

        private:
            map_type map_;
        };
    }
}

#endif
