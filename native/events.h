#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "base.h"
#include "ext/FastDelegate.h"

namespace dev
{
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

    namespace internal
    {
        template<class R=fastdelegate::detail::DefaultVoid, class ...P>
        class fast_delegate_base {
        private:
            typedef typename fastdelegate::detail::DefaultVoidToVoid<R>::type desired_ret_t;
            typedef desired_ret_t (*static_func_ptr)(P...);
            typedef R (*unvoid_static_func_ptr)(P...);
            typedef R (fastdelegate::detail::GenericClass::*generic_mem_fn)(P...);
            typedef fastdelegate::detail::ClosurePtr<generic_mem_fn, static_func_ptr, unvoid_static_func_ptr> closure_t;
            closure_t closure_;
        public:
            // Typedefs to aid generic programming
            typedef fast_delegate_base type;

            // Construction and comparison functions
            fast_delegate_base() { clear(); }

            fast_delegate_base(const fast_delegate_base &x)
            {
                closure_.CopyFrom(this, x.closure_);
            }

            void operator = (const fast_delegate_base &x)
            {
                closure_.CopyFrom(this, x.closure_);
            }
            bool operator ==(const fast_delegate_base &x) const
            {
                return closure_.IsEqual(x.closure_);
            }
            bool operator !=(const fast_delegate_base &x) const
            {
                return !closure_.IsEqual(x.closure_);
            }
            bool operator <(const fast_delegate_base &x) const
            {
                return closure_.IsLess(x.closure_);
            }
            bool operator >(const fast_delegate_base &x) const
            {
                return x.closure_.IsLess(closure_);
            }

            // Binding to non-const member functions
            template<class X, class Y>
            fast_delegate_base(Y *pthis, desired_ret_t (X::* function_to_bind)(P...) )
            {
                closure_.bindmemfunc(fastdelegate::detail::implicit_cast<X*>(pthis), function_to_bind);
            }

            template<class X, class Y>
            inline void bind(Y *pthis, desired_ret_t (X::* function_to_bind)(P...))
            {
                closure_.bindmemfunc(fastdelegate::detail::implicit_cast<X*>(pthis), function_to_bind);
            }

            // Binding to const member functions.
            template<class X, class Y>
            fast_delegate_base(const Y *pthis, desired_ret_t (X::* function_to_bind)(P...) const)
            {
                closure_.bindconstmemfunc(fastdelegate::detail::implicit_cast<const X*>(pthis), function_to_bind);
            }

            template<class X, class Y>
            inline void bind(const Y *pthis, desired_ret_t (X::* function_to_bind)(P...) const)
            {
                closure_.bindconstmemfunc(fastdelegate::detail::implicit_cast<const X *>(pthis), function_to_bind);
            }

            // Static functions. We convert them into a member function call.
            // This constructor also provides implicit conversion
            fast_delegate_base(desired_ret_t (*function_to_bind)(P...) )
            {
                bind(function_to_bind);
            }

            // for efficiency, prevent creation of a temporary
            void operator = (desired_ret_t (*function_to_bind)(P...) )
            {
                bind(function_to_bind);
            }

            inline void bind(desired_ret_t (*function_to_bind)(P...))
            {
                closure_.bindstaticfunc(this, &fast_delegate_base::invoke_static_func<P...>, function_to_bind);
            }

            // Invoke the delegate
            template<typename ...A>
            R operator()(A&&... args) const
            {
                return (closure_.GetClosureThis()->*(closure_.GetClosureMemPtr()))(std::forward<A>(args)...);
            }
            // Implicit conversion to "bool" using the safe_bool idiom

        private:
            typedef struct safe_bool_struct
            {
                int a_data_pointer_to_this_is_0_on_buggy_compilers;
                static_func_ptr m_nonzero;
            } useless_typedef;
            typedef static_func_ptr safe_bool_struct::*unspecified_bool_type;
        public:
            operator unspecified_bool_type() const { return empty()? 0: &safe_bool_struct::m_nonzero; }
            // necessary to allow ==0 to work despite the safe_bool idiom
            inline bool operator==(static_func_ptr funcptr) { return closure_.IsEqualToStaticFuncPtr(funcptr); }
            inline bool operator!=(static_func_ptr funcptr) { return !closure_.IsEqualToStaticFuncPtr(funcptr); }
            // Is it bound to anything?
            inline bool operator ! () const { return !closure_; }
            inline bool empty() const { return !closure_; }
            void clear() { closure_.clear();}
            // Conversion to and from the DelegateMemento storage class
            const fastdelegate::DelegateMemento & GetMemento() { return closure_; }
            void SetMemento(const fastdelegate::DelegateMemento &any) { closure_.CopyFrom(this, any); }

        private:
            // Invoker for static functions
            template<typename ...A>
            R invoke_static_func(A&&... args) const
            {
                return (*(closure_.GetStaticFunction()))(std::forward<A>(args)...);
            }
        };

        // fast_delegate<> is similar to std::function, but it has comparison operators.
        template<typename _Signature>
        class fast_delegate;

        template<typename R, typename ...P>
        class fast_delegate<R(P...)> : public fast_delegate_base<R, P...>
        {
        public:
            typedef fast_delegate_base<R, P...> BaseType;

            fast_delegate() : BaseType() { }

            template<class X, class Y>
            fast_delegate(Y * pthis, R (X::* function_to_bind)(P...))
                : BaseType(pthis, function_to_bind)
            { }

            template<class X, class Y>
            fast_delegate(const Y *pthis, R (X::* function_to_bind)(P...) const)
                : BaseType(pthis, function_to_bind)
            { }

            fast_delegate(R (*function_to_bind)(P...))
                : BaseType(function_to_bind)
            { }

            void operator = (const BaseType &x)
            {
                *static_cast<BaseType*>(this) = x;
            }
        };

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
        typedef std::list<std::shared_ptr<internal::callback_obj_base>> listener_list;

    public:
        EventEmitter()
            : callback_lut_()
        {}
        virtual ~EventEmitter()
        {}

    public:
        template<typename T, int event_id> struct callback_type {};

    public:

        template<typename T, int event_id>
        void on(typename callback_type<T, event_id>::type callback)
        {
            addListener<T, event_id>(callback);
        }

        // TODO: addListener() takes class type as its first template parameter.
        template<typename T, int event_id>
        void addListener(typename callback_type<T, event_id>::type callback)
        {
            auto it = callback_lut_.find(event_id);
            if(it == callback_lut_.end()) callback_lut_.insert(std::make_pair(event_id, listener_list()));
            callback_lut_[event_id].push_back(std::shared_ptr<internal::callback_obj_base>(
                new internal::callback_obj<typename callback_type<T, event_id>::type>(callback)));
        }

        template<typename T, int event_id>
        void once(typename callback_type<T, event_id>::type callback)
        {
            // TODO: not implemented - EventEmitter::once()
        }

        template<typename T, int event_id, typename ...A>
        void emit(A&& ... args)
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
