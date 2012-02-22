#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"

namespace native
{
    class Exception;
    class Stream;
    class Buffer;
    namespace net { class Socket; }

    /**
     *  All event types are defined under native::event namespace.
     */
    namespace event
    {
        /**
         *  @brief 'exit' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct exit : public util::callback_def<> {};
        /**
         *  @brief 'uncaughtException' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(const Exception& exception) @endcode
         *
         *  @param exception        The Exception object.
         */
        struct uncaughtException : public util::callback_def<const Exception&> {};
        /**
         *  @brief 'data' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(const Buffer& buffer) @endcode
         *
         *  @param buffer           The Buffer object that contains the data.
         */
        struct data : public util::callback_def<const Buffer&> {};
        /**
         *  @brief 'end' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct end : public util::callback_def<> {};
        /**
         *  @brief 'error' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(const Exception& exception) @endcode
         *
         *  @param exception        The Exception object.
         */
        struct error : public util::callback_def<const Exception&> {};
        /**
         *  @brief 'close' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct close : public util::callback_def<> {};
        /**
         *  @brief 'drain' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct drain : public util::callback_def<> {};
        /**
         *  @brief 'pipe' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(Stream* stream) @endcode
         *
         *  @param stream       The source Stream object.
         */
        struct pipe : public util::callback_def<Stream*> {};
        /**
         *  @brief 'secure' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct secure : public util::callback_def<> {};
        /**
         *  @brief 'secureConnection' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(Stream* stream) @endcode
         *
         *  @param stream       ...
         */
        struct secureConnection : public util::callback_def<Stream*> {};
        /**
         *  @brief 'clientError' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(const Exception& exception) @endcode
         *
         *  @param exception       The Exception object.
         */
        struct clientError : public util::callback_def<const Exception&> {};
        /**
         *  @brief 'secureConnect' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct secureConnect : public util::callback_def<> {};
        /**
         *  @brief 'open' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(int fd) @endcode
         *
         *  @param fd               File descriptor.
         */
        struct open : public util::callback_def<int> {};
        /**
         *  @brief 'change' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(int fd, const std::string& p) @endcode
         *
         *  @param fd               File descriptor.
         *  @param p                ...
         */
        struct change : public util::callback_def<int, const std::string&> {};
        /**
         *  @brief 'listening' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct listening : public util::callback_def<> {};
        /**
         *  @brief 'data' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback(const Buffer& buffer) @endcode
         *
         *  @param buffer           The Buffer object that contains the data.
         */
        struct connection : public util::callback_def<net::Socket*> {};
        /**
         *  @brief 'connect' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
        struct connect: public util::callback_def<> {};
        /**
         *  @brief 'timeout' event.
         *
         *  @remark
         *  Callback function has the following signature.
         *  @code void callback() @endcode
         */
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
        listener_t addListener(const typename E::callback_type& callback)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                return s->second.add<typename E::callback_type>(callback);
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
        listener_t on(const typename E::callback_type& callback)
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
        listener_t once(const typename E::callback_type& callback)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                return s->second.add<typename E::callback_type>(callback, true);
            }

            return nullptr;
        }

        /**
         *  Removes a listener for the event.
         *
         *  @param listener     A unique value for the listener. You must use the return value of addListener(), on(), or once() function.
         *
         *  @retval true       The listener was removed from EventEmitter.
         *  @retval false      The listener was not found in EventEmitter, or the event was not registered.
         */
        template<typename E>
        bool removeListener(listener_t listener)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                return s->second.remove(listener);
            }

            return false;
        }

        /**
         *  Removes all listeners for the event.
         *
         *  @retval true       All the listeners for the event were removed.
         *  @retval false      The event was not registered.
         */
        template<typename E>
        bool removeAllListeners()
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                s->second.clear();
                return true;
            }

            return false;
        }

        /**
         *  Invokes callbacks for the event.
         *
         *  @param args        Arguments for the callbacks.
         *
         *  @retval true       All the callbacks were invoked.
         *  @retval false      The event was not registered.
         */
        template<typename E, typename ...A>
        bool emit(A&&... args)
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                s->second.invoke<typename E::callback_type>(std::forward<A>(args)...);
                return true;
            }

            return false;
        }

        /**
         *  Tests if one or more listeners are added for the event.
         *
         *  @retval true       One or more listeners are added for the event.
         *  @retval false      No listener is added, or the event was not registered.
         */
        template<typename E>
        bool haveListener() const
        {
            auto s = events_.find(typeid(E).hash_code());
            if(s != events_.end())
            {
                return s->second.count() > 0;
            }

            return false;
        }

    protected:
        /**
         *  Registers a new event.
         *
         *  @retval true       The event was newly registered.
         *  @retval false      The same event was already registered.
         */
        template<typename E>
        bool registerEvent()
        {
            auto res = events_.insert(std::make_pair(typeid(E).hash_code(), detail::event_emitter()));
            if(!res.second)
            {
                // Two event IDs conflict
                return false;
            }
            return true;
        }

        /**
         *  Unegisters the event.
         *  @retval true       The event was unregistered.
         *  @retval false      The event was not registered.
         */
        template<typename E>
        bool unregisterEvent()
        {
            return events_.erase(typeid(E).hash_code()) > 0;
        }

    private:
        //std::map<int, std::shared_ptr<detail::sigslot_base>> events_;
        std::map<int, detail::event_emitter> events_;
    };
}

#endif
