#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"
#include "utility.h"

namespace native
{
    class Exception;
    class Stream;
    class Buffer;
    namespace net { class Socket; }

    namespace ev
    {
        struct exit : public util::callback_def<> {};
        struct uncaughtException : public util::callback_def<const Exception&> {};
        // TODO: hmm... event id(int) and listner(void*)
        struct newListener : public util::callback_def<int, void*> {};
        struct data : public util::callback_def<const Buffer&> {};
        struct end : public util::callback_def<> {};
        struct error : public util::callback_def<Exception> {};
        struct close : public util::callback_def<> {};
        struct close2 : public util::callback_def<bool> {};
        struct drain : public util::callback_def<> {};
        struct pipe : public util::callback_def<Stream*> {};
        struct secure : public util::callback_def<> {};
        struct secureConnection : public util::callback_def<Stream*> {};
        struct clientError : public util::callback_def<const Exception&> {};
        struct secureConnect : public util::callback_def<> {};
        struct open : public util::callback_def<int> {};
        struct change : public util::callback_def<int, const std::string&> {};
        struct listening : public util::callback_def<> {};
        struct connection : public util::callback_def<net::Socket*> {};
        struct connect: public util::callback_def<> {};
        struct timeout: public util::callback_def<> {};
    }

    class EventEmitter
    {
    protected:
        EventEmitter() : events_() {}

    public:
        virtual ~EventEmitter() {}

    public:
        typedef void* listener_t;

        template<typename E>
        listener_t addListener(typename E::callback_type callback)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                auto ptr = new (decltype(callback))(callback);
                s->second->add_callback(ptr);
                return ptr;
            }

            return nullptr;
        }

        template<typename E>
        listener_t on(typename E::callback_type callback)
        {
            return addListener<E>(callback);
        }

        template<typename E>
        bool removeListener(listener_t listener)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                return s->second->remove_callback(listener);
            }

            return false;
        }

        template<typename E>
        bool removeAllListeners()
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                s->second->reset();
                return true;
            }

            return false;
        }

        template<typename E, typename ...A>
        bool emit(A&&... args)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                auto x = dynamic_cast<detail::sigslot<typename E::callback_type>*>(s->second.get());
                assert(x);

                x->invoke(std::forward<A>(args)...);
                return true;
            }

            return false;
        }

        template<typename E>
        bool haveListener() const
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                return s->second->callback_count() > 0;
            }

            return false;
        }

    protected:
        template<typename E>
        bool registerEvent()
        {
            auto res = events_.insert(std::make_pair(
                typeid(E).hash_code(),
                std::shared_ptr<detail::sigslot_base>(
                    new detail::sigslot<typename E::callback_type>())));
            if(!res.second)
            {
                // Two event IDs conflict
                return false;
            }
            return true;
        }

        template<typename E>
        bool unregisterEvent()
        {
            return events_.erase(typeid(E).hash_code()) > 0;
        }

    private:
        std::map<int, std::shared_ptr<detail::sigslot_base>> events_;
    };
}

#endif
