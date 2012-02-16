#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"
#include "utility.h"

namespace native
{
    namespace ev
    {
        class Exception;
        class Stream;

        struct exit
            : public std::integral_constant<int, 0>
            , public util::callback_def<> {};

        struct uncaughtException
            : public std::integral_constant<int, 19>
            , public util::callback_def<const Exception&> {};

        // TODO: hmm... event id(int) and listner(void*)
        struct newListener
            : public std::integral_constant<int, 19>
            , public util::callback_def<int, void*> {};

        struct data
            : public std::integral_constant<int, 19>
            , public util::callback_def<const std::vector<char>&> {};

        struct end
            : public std::integral_constant<int, 19>
            , public util::callback_def<> {};

        struct error
            : public std::integral_constant<int, 19>
            , public util::callback_def<const Exception&> {};

        struct close
            : public std::integral_constant<int, 20>
            , public util::callback_def<> {};

        struct close2
            : public std::integral_constant<int, 20>
            , public util::callback_def<bool> {};

        struct drain
            : public std::integral_constant<int, 19>
            , public util::callback_def<> {};

        struct pipe
            : public std::integral_constant<int, 19>
            , public util::callback_def<Stream&> {};

        struct secure
            : public std::integral_constant<int, 19>
            , public util::callback_def<> {};

        struct secureConnection
            : public std::integral_constant<int, 19>
            , public util::callback_def<Stream&> {};

        struct clientError
            : public std::integral_constant<int, 19>
            , public util::callback_def<const Exception&> {};

        struct secureConnect
            : public std::integral_constant<int, 19>
            , public util::callback_def<> {};

        struct open
            : public std::integral_constant<int, 19>
            , public util::callback_def<int> {};

        struct change
            : public std::integral_constant<int, 19>
            , public util::callback_def<int, const std::string&> {};

        struct listening
            : public std::integral_constant<int, 19>
            , public util::callback_def<> {};
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
            auto s = events_.find(E::value);
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
            auto s = events_.find(E::value);
            if(s != events_.end())
            {
                return s->second->remove_callback(listener);
            }

            return false;
        }

        template<typename E>
        bool removeAllListeners()
        {
            auto s = events_.find(E::value);
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
            auto s = events_.find(E::value);
            if(s != events_.end())
            {
                auto x = dynamic_cast<detail::sigslot<typename E::callback_type>*>(s->second.get());
                assert(x);

                x->invoke(std::forward<A>(args)...);
                return true;
            }

            return false;
        }

    protected:
        template<typename E>
        bool registerEvent()
        {
            auto res = events_.insert(std::make_pair(
                E::value,
                std::shared_ptr<detail::sigslot_base>(
                    new detail::sigslot<typename E::callback_type>())));
            if(!res.second)
            {
                // Two event IDs conflict:
                //  1) different event type with the same ::value
                //  2) second registration of the same event type.
                assert(false);
                return false;
            }
            return true;
        }

        template<typename E>
        bool unregisterEvent()
        {
            return events_.erase(E::value) > 0;
        }

    private:
        std::map<int, std::shared_ptr<detail::sigslot_base>> events_;
    };
}

#endif
