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

    /**
     *  EventEmitter class.
     */
    class EventEmitter
    {
    protected:
        /**
         *  To create an EventEmitter object, you must define a new class inheriting from EventEmitter.
         *  Before you can call addListener() or emit() function, you have to register the events that you want to accept.
         *  Typically, you can register events in your class contructor.
         */
        EventEmitter()
            : events_()
        {}

    public:
        virtual ~EventEmitter()
        {}

    public:
        /**
         *  addListener(), on(), and once() function returns listener_t object which is just an untyped pointer.
         *  If you're going to remove a listener from EventEmitter, you have to save this returned value.
         */
        typedef void* listener_t;

        /**
         *  Adds a listener for the event.
         *
         *  @param callback     Callback object.
         *
         *  @return             A unique value for the listener. This value is used as a parameter for removeListener() function.
         *                      Even if you add the same callback object, this return value differ for each calls.
         *                      If EventEmitter fails to add the listener, it returns nullptr.
         */
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

        /**
         *  Adds a listener for the event.
         *
         *  @param callback     Callback object.
         *
         *  @return             A unique value for the listener. This value is used as a parameter for removeListener() function.
         *                      Even if you add the same callback object, this return value differ for each calls.
         *                      If EventEmitter fails to add the listener, it returns nullptr.
         */
        template<typename E>
        listener_t on(typename E::callback_type callback)
        {
            return addListener<E>(callback);
        }

        /**
         *  Adds a listener for the event. This listener is deleted after it is invoked first time.
         *
         *  @param callback     Callback object.
         *
         *  @return             A unique value for the listener. This value is used as a parameter for removeListener() function.
         *                      Even if you add the same callback object, this return value differ for each calls.
         *                      If EventEmitter fails to add the listener, it returns nullptr.
         */
        template<typename E>
        listener_t once(typename E::callback_type callback)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                auto ptr = new (decltype(callback))(callback);
                s->second->add_callback(ptr, true);
                return ptr;
            }

            return nullptr;
        }

        /**
         *  Removes a listener for the event.
         *
         *  @param listener     A unique value for the listener. You must use the return value of addListener(), on(), or once() function.
         *
         *  @ret_val true       The listener was removed from EventEmitter.
         *  @ret_val false      The listener was not found in EventEmitter, or the event was not registered.
         */
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

        /**
         *  Removes all listeners for the event.
         *
         *  @ret_val true       All the listeners for the event were removed.
         *  @ret_val false      The event was not registered.
         */
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

        /**
         *  Invokes callbacks for the event.
         *
         *  @ret_val true       All the callbacks were invoked.
         *  @ret_val false      The event was not registered.
         */
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

        /**
         *  Tests if one or more listeners are added for the event.
         *
         *  @ret_val true       One or more listeners are added for the event.
         *  @ret_val false      No listener is added, or the event was not registered.
         */
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
        /**
         *  Registers a new event.
         */
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

        /**
         *  Unegisters the event.
         */
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
