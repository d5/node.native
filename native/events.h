#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"

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

    namespace internal
    {
        class sigslot_base
        {
        public:
            sigslot_base() {}
            virtual ~sigslot_base() {}

            virtual void add_callback(void*) = 0;
            virtual void remove_callback(void*) = 0;
            virtual void reset() = 0;
        };

        template<typename ...P>
        class sigslot: public sigslot_base
        {
        public:
            typedef std::function<void(P...)> callback_type;
            typedef std::shared_ptr<callback_type> callback_ptr;

        public:
            sigslot()
                : callbacks_()
            {}
            virtual ~sigslot()
            {}

        public:
            virtual void add_callback(void* callback)
            {
                callbacks_.insert(callback_ptr(reinterpret_cast<callback_type*>(callback)));
            }

            virtual void remove_callback(void* callback)
            {
                callbacks_.erase(callback_ptr(reinterpret_cast<callback_type*>(callback)));
            }

            virtual void reset()
            {
                callbacks_.clear();
            }

            void invoke(P&&... args)
            {
                for(auto c : callbacks_) (*c)(std::forward<P>(args)...);
            }

        private:
            std::set<callback_ptr> callbacks_;
        };
    }

    class EventEmitter
    {
    protected:
        EventEmitter() : events_() {}

    public:
        virtual ~EventEmitter() {}

        typedef void* listener_t;

        template<typename ...A>
        listener_t addListener(
            int event_id,
            typename internal::sigslot<A...>::callback_type callback)
        {
            auto s = events_.find(event_id);
            if(s != events_.end())
            {
                // test if function signature matches or not.
                assert(dynamic_cast<typename internal::sigslot<A...>*>(s->second.get()));

                auto ptr = new typename internal::sigslot<A...>::callback_type(callback);
                s->second->add_callback(ptr);
                return ptr;
            }
            else
            {
                throw "Not registered event.";
            }
        }

        void removeListener(int event_id, listener_t listener)
        {
            auto s = events_.find(event_id);
            if(s != events_.end())
            {
                s->second->remove_callback(listener);
            }
            else
            {
                throw "Not registered event.";
            }
        }

        void reset(int event_id)
        {
            auto s = events_.find(event_id);
            if(s != events_.end())
            {
                s->second->reset();
            }
            else
            {
                throw "Not registered event.";
            }
        }

        template<typename ...A>
        void operator()(int event_id, A&&... args)
        {
            emit(event_id, std::forward<A>(args)...);
        }

        template<typename ...A>
        void emit(int event_id, A&&... args)
        {
            auto s = events_.find(event_id);
            if(s != events_.end())
            {
                auto x = dynamic_cast<internal::sigslot<A...>*>(s->second.get());
                if(x)
                {
                    x->invoke(std::forward<A>(args)...);
                }
                else
                {
                    throw "Invalid callback type.";
                }
            }
            else
            {
                throw "Not registered event.";
            }
        }

    protected:
        template<typename ...A>
        void registerEvent(int event_id)
        {
            auto it = events_.find(event_id);
            if(it != events_.end())
            {
                throw "Event is already registered.";
            }

            events_.insert(std::make_pair(
                event_id,
                std::shared_ptr<internal::sigslot_base>(new internal::sigslot<A...>())));
        }

        void unregisterEvent(int event_id)
        {
            events_.erase(event_id);
        }

    private:
        std::map<int, std::shared_ptr<internal::sigslot_base>> events_;
    };
}

#endif
