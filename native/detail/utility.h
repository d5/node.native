#ifndef __DETAIL_UTILITY_H__
#define __DETAIL_UTILITY_H__

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

        namespace text
        {
            void lower(std::string& str)
            {
                std::for_each(str.begin(), str.end(), [](char& c){ c = tolower(c); });
            }

            void upper(std::string& str)
            {
                std::for_each(str.begin(), str.end(), [](char& c){ c = toupper(c); });
            }

            std::string to_lower(const std::string& str)
            {
                std::string res(str);
                std::for_each(res.begin(), res.end(), [](char& c){ c = tolower(c); });
                return res;
            }

            std::string to_upper(const std::string& str)
            {
                std::string res(str);
                std::for_each(res.begin(), res.end(), [](char& c){ c = toupper(c); });
                return res;
            }

            struct ci_less : std::binary_function<std::string, std::string, bool>
            {
                struct nocase_compare : public std::binary_function<unsigned char, unsigned char, bool>
                {
                    bool operator()(const unsigned char& c1, const unsigned char& c2) const
                    {
                        return tolower(c1) < tolower(c2);
                    }
                };

                bool operator()(const std::string & s1, const std::string & s2) const
                {
                    return std::lexicographical_compare(s1.begin(), s1.end(), // source range
                        s2.begin(), s2.end(), // dest range
                        nocase_compare()); // comparison
                }
            };

            bool compare_no_case(const std::string& s1, const std::string& s2)
            {
                // TODO: need optimization
                return to_lower(s1).compare(to_lower(s2)) == 0;
            }
        }

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
            typedef std::unordered_map<std::string, std::string> map_type;
            typedef typename map_type::iterator iterator;
            typedef typename map_type::const_iterator const_iterator;

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
                return default_value;
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

            bool compare_no_case(const std::string& name, const std::string& value) const
            {
                auto it = map_.find(name);
                if(it == map_.end()) return false;
                return text::compare_no_case(value, it->second);
            }

            bool has(const std::string& name) const
            {
                return map_.count(name) > 0;
            }

            bool remove(const std::string& name)
            {
                return map_.erase(name) > 0;
            }

            std::size_t size() const { return map_.size(); }

            iterator begin() { return map_.begin(); }
            const_iterator begin() const { return map_.begin(); }
            iterator end() { return map_.end(); }
            const_iterator end() const { return map_.end(); }
            const_iterator cbegin() const { return map_.cbegin(); }
            const_iterator cend() const { return map_.cend(); }

        private:
            map_type map_;
        };
    }
}

#endif
