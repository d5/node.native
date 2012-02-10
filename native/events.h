#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"

namespace dev
{
    namespace internal
    {
        class callback_obj_base
        {
        public:
            callback_obj_base() {}
            virtual ~callback_obj_base() {}
        };

        template<typename callback_t>
        class callback_obj : public callback_obj_base
        {
        public:
            callback_obj(callback_t callback)
                : callback_(callback)
            {}

            template<typename ...A>
            void invoke(A&& ... args)
            {
                callback_(std::forward<A>(args)...);
            }

        private:
            callback_t callback_;
        };
    }

    class EventEmitter
    {
        typedef std::vector<std::shared_ptr<internal::callback_obj_base>> listener_list;

    public:
        EventEmitter()
            : callback_lut_()
        {}
        virtual ~EventEmitter()
        {}

    public:
        template<typename T, int event_id> struct callback_type {};

    protected:
        template<typename T, int event_id>
        void addListener_(typename callback_type<T, event_id>::type callback)
        {
            auto it = callback_lut_.find(event_id);
            if(it == callback_lut_.end()) callback_lut_.insert(std::make_pair(event_id, listener_list()));
            callback_lut_[event_id].push_back(std::shared_ptr<internal::callback_obj_base>(new internal::callback_obj<typename callback_type<T, event_id>::type>(callback)));
        }

        template<typename T, int event_id, typename ...A>
        void invokeEvent_(A&& ... args)
        {
            typedef typename callback_type<T, event_id>::type cb_t;
            typedef internal::callback_obj<cb_t> ev_t;

            auto e = callback_lut_.find(event_id);
            if(e != callback_lut_.end())
            {
                for(auto l : e->second)
                {
                    auto f = dynamic_cast<ev_t*>(l.get());
                    assert(f);
                    f->invoke(std::forward<A>(args)...);
                }
            }
        }

    private:
        std::map<int, listener_list> callback_lut_;
    };

    struct event
    {
        enum type
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
        };
    };

#if 0
    // SAMPLE EXTENSION
    class class1;
    template<> struct EventEmitter::callback_type<class1, event::debug1> { typedef std::function<void(int)> type; };
    template<> struct EventEmitter::callback_type<class1, event::debug2> { typedef std::function<void(int, int)> type; };

    class class1 : public EventEmitter
    {
    public:
        // TODO: addListener() and invokeEvent() are not virtual functions.
        template<int event_id>
        void addListener(typename callback_type<class1, event_id>::type callback)
        {
            addListener_<class1, event_id>(callback);
        }

        template<int event_id, typename ...A>
        void invokeEvent(A&& ... args)
        {
            invokeEvent_<class1, event_id>(std::forward<A>(args)...);
        }
    };
#endif
}

#endif
