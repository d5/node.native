#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"
#include "utility.h"

namespace dev
{
    struct event
    {
        enum
        {
            none,
            exit,
            uncaughtException,
            newListener,
            data,
            end,
            error,
            close,
            drain,
            pipe,
            secure,
            secureConnection,
            clientError,
            secureConnect,
            open,
            change,
            listening,
            connection,
            connect,
            timeout,
            message,
            request,
            checkContinue,
            upgrade,
            response,
            socket,
            continue_,
            line,
            death,
            debug1,
            debug2,
            user_defined
        };
    };

    template<typename ...F>
    struct EventMap
    {
        typedef typename utility::tuple_even_elements<F...>::type events;
        typedef typename utility::tuple_odd_elements<F...>::type callbacks;
    };

    template<typename map>
    class EventEmitter
    {
        typedef typename map::events events;
        typedef typename map::callbacks callbacks;

        template<typename e>
        struct evt_idx : public std::integral_constant<std::size_t, utility::tuple_index_of<events, e>::value> {};

        template<typename e>
        struct cb_def
        { typedef typename std::tuple_element<evt_idx<e>::value, callbacks>::type type; };

    public:
        typedef void* listener_t;

        EventEmitter()
            : set_()
        {}

        virtual ~EventEmitter()
        {
        }

        template<typename event>
        auto addListener(typename cb_def<event>::type callback) -> decltype(&callback)
        {
            auto& entry = std::get<evt_idx<event>::value>(set_);

            auto ptr = new decltype(callback)(callback);
            entry.push_back(std::shared_ptr<decltype(callback)>(ptr));
            return ptr;
        }

        template<typename event>
        bool removeListener(typename cb_def<event>::type* callback_ptr)
        {
            auto& entry = std::get<evt_idx<event>::value>(set_);

            auto it = entry.begin();
            for(;it!=entry.end();++it)
            {
                if(it->get() == callback_ptr)
                {
                    entry.erase(it);
                    return true;
                }
            }
            return false;
        }

        template<typename event>
        void removeAllListeners()
        {
            auto& entry = std::get<evt_idx<event>::value>(set_);
            entry.clear();
        }

        template<typename event, typename ...A>
        void emit(A&&... args)
        {
            auto& entry = std::get<evt_idx<event>::value>(set_);
            for(auto x : entry) (*x)(std::forward<A>(args)...);
        }

    private:
        template<typename>
        struct set_t;

        template<typename ...T>
        struct set_t<std::tuple<T...>>
        { typedef std::tuple<std::list<std::shared_ptr<T>>...> type; };

        typename set_t<callbacks>::type set_;
    };
}

#endif
