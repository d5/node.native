#ifndef __DETAIL_BASE_H__
#define __DETAIL_BASE_H__

#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <list>
#include <set>
#include <tuple>
#include <iostream>
#include <arpa/inet.h>
#include <uv.h>
#include <http_parser.h>

namespace native
{
    namespace error
    {
        enum error_code_t
        {
            unknown = UV_UNKNOWN,
            ok = UV_OK,
            eof = UV_EOF,
            eaddrinfo = UV_EADDRINFO,
            eacces = UV_EACCES,
            eagain = UV_EAGAIN,
            eaddrinuse = UV_EADDRINUSE,
            eaddrnotavail = UV_EADDRNOTAVAIL,
            eafnosupport = UV_EAFNOSUPPORT,
            ealready = UV_EALREADY,
            ebadf = UV_EBADF,
            ebusy = UV_EBUSY,
            econnaborted = UV_ECONNABORTED,
            econnrefused = UV_ECONNREFUSED,
            econnreset = UV_ECONNRESET,
            edestaddrreq = UV_EDESTADDRREQ,
            efault = UV_EFAULT,
            ehostunreach = UV_EHOSTUNREACH,
            eintr = UV_EINTR,
            einval = UV_EINVAL,
            eisconn = UV_EISCONN,
            emfile = UV_EMFILE,
            emsgsize = UV_EMSGSIZE,
            enetdown = UV_ENETDOWN,
            enetunreach = UV_ENETUNREACH,
            enfile = UV_ENFILE,
            enobufs = UV_ENOBUFS,
            enomem = UV_ENOMEM,
            enotdir = UV_ENOTDIR,
            eisdir = UV_EISDIR,
            enonet = UV_ENONET,
            enotconn = UV_ENOTCONN,
            enotsock = UV_ENOTSOCK,
            enotsup = UV_ENOTSUP,
            enoent = UV_ENOENT,
            enosys = UV_ENOSYS,
            epipe = UV_EPIPE,
            eproto = UV_EPROTO,
            eprotonosupport = UV_EPROTONOSUPPORT,
            eprototype = UV_EPROTOTYPE,
            etimedout = UV_ETIMEDOUT,
            echarset = UV_ECHARSET,
            eaifamnosupport = UV_EAIFAMNOSUPPORT,
            eaiservice = UV_EAISERVICE,
            eaisocktype = UV_EAISOCKTYPE,
            eshutdown = UV_ESHUTDOWN,
            eexist = UV_EEXIST,
            esrch = UV_ESRCH,
            enametoolong = UV_ENAMETOOLONG,
            eperm = UV_EPERM,
            eloop = UV_ELOOP,
            exdev = UV_EXDEV,
            __libuv_max = UV_MAX_ERRORS,

            http_parser_fail,
            http_parser_url_fail,
            http_parser_incomplete,

            reserved,
        };
    }

    namespace detail
    {
        enum cid
        {
            cid_uv_close,
            cid_uv_listen,
            cid_uv_read_start,
            cid_uv_read2_start,
            cid_uv_write,
            cid_uv_write2,
            cid_uv_shutdown,
            cid_uv_connect,
            cid_uv_connect6,
            cid_max
        };

        enum encoding_type
        {
            et_ascii,
            et_utf8,
            et_ucs2,
            et_base64,
            et_binary,
            et_hex,
            et_none
        };

        int get_encoding_code(const std::string& name)
        {
            if(name == "ascii") return et_ascii;
            else if(name == "utf8") return et_utf8;
            else if(name == "ucs2") return et_ucs2;
            else if(name == "base64") return et_base64;
            else if(name == "binary") return et_binary;
            else if(name == "hex") return et_hex;
            else return et_none;
        }

        class resval
        {
        public:
            resval(int error_code=error::ok) : error_code_(error_code) {}
            explicit resval(uv_err_t e) : error_code_(e.code) {}
            explicit resval(uv_err_code code) : error_code_(code) {}
            resval(const resval& c) : error_code_(c.error_code_) {}
            resval(resval&& c) : error_code_(c.error_code_) {}
            ~resval() {}

            operator bool() const { return error_code_ == error::ok; }
            bool operator !() const { return  error_code_ != error::ok; }

            int code() const
            {
                return error_code_;
            }

            const char* str() const
            {
                if(error_code_ < static_cast<int>(error::__libuv_max))
                {
                    return uv_strerror(uv_err_t{static_cast<uv_err_code>(error_code_), 0});
                }

                switch(error_code_)
                {
                case error::http_parser_fail: return "Failed to parser HTTP request or response.";
                case error::http_parser_url_fail: return "Failed to parse URL elements in HTTP request or response.";
                case error::http_parser_incomplete: return "HTTP request or response message is not complete.";
                default: return nullptr;
                }
            }
        private:
            int error_code_;
        };

        struct net_addr
        {
            bool is_ipv4;
            std::string ip;
            int port;
        };

        int get_ip_version(const std::string& ip)
        {
            if(ip.empty()) return 0;

            unsigned char buf[sizeof(in6_addr)];

            if(inet_pton(AF_INET, ip.c_str(), buf) == 1) return 4;
            else if(inet_pton(AF_INET6, ip.c_str(), buf) == 1) return 6;

            return 0;
        }

        resval get_last_error() { return resval(uv_last_error(uv_default_loop())); }

        template<typename F, typename ...A>
        resval run_(F fn, A&&... args)
        {
            static_assert(std::is_pointer<F>::value, "Template parameter F is not a plain-function.");
            static_assert(std::is_same<decltype(fn(args...)), int>::value, "Return value of template parameter F is not int.");

            return fn(std::forward<A>(args)...) ? get_last_error() : resval();
        }

        sockaddr_in to_ip4_addr(const std::string& ip, int port) { return uv_ip4_addr(ip.c_str(), port); }
        sockaddr_in6 to_ip6_addr(const std::string& ip, int port) { return uv_ip6_addr(ip.c_str(), port); }

        resval from_ip4_addr(sockaddr_in* src, std::string& ip, int& port)
        {
            char dest[16];
            if(uv_ip4_name(src, dest, 16)) get_last_error();

            ip = dest;
            port = static_cast<int>(ntohs(src->sin_port));
            return resval();
        }

        resval from_ip6_addr(sockaddr_in6* src, std::string& ip, int& port)
        {
            char dest[46];
            if(uv_ip6_name(src, dest, 46)) get_last_error();

            ip = dest;
            port = static_cast<int>(ntohs(src->sin6_port));
            return resval();
        }

        class scl_base
        {
        public:
            scl_base() {}
            virtual ~scl_base() {}
        public:
            virtual void add(void*, bool once=false) = 0;
            virtual bool remove(void*) = 0;
            virtual void clear() = 0;
            virtual std::size_t count() const = 0;
        };

        // serialized callback list
        template<typename>
        class scl;

        template<typename R, typename ...P>
        class scl<std::function<R(P...)>> : public scl_base
        {
        public:
            typedef std::function<R(P...)> callback_type;
            typedef std::shared_ptr<callback_type> callback_ptr;

        public:
            scl()
                : list_()
            {}
            virtual ~scl()
            {}

        public:
            virtual void add(void* callback, bool once=false)
            {
                assert(callback);

                // wrap the callback object with shared_ptr<>.
                list_.push_back(std::make_pair(callback_ptr(reinterpret_cast<callback_type*>(callback)), once));
            }

            virtual bool remove(void* callback)
            {
                // find the matching callback
                auto d = list_.end();
                for(auto it=list_.begin();it!=list_.end();++it)
                {
                    if(reinterpret_cast<void*>(it->first.get()) == callback)
                    {
                        d = it;
                        break;
                    }
                }

                // if found: delete it from the list.
                if(d != list_.end())
                {
                    list_.erase(d);
                    return true;
                }

                // failed to find the callback
                return false;
            }

            virtual void clear()
            {
                // delete all callbacks
                list_.clear();
            }

            virtual std::size_t count() const
            {
                // the number of callbacks
                return list_.size();
            }

            template<typename ...A>
            void invoke(A&&... args)
            {
                // set of callbacks to delete after execution.
                std::set<callback_ptr> to_delete;

                // copy callback list: to avoid modification inside the 'for' loop.
                auto callbacks_copy = list_;
                for(auto c : callbacks_copy)
                {
                    try
                    {
                        // execute the callback
                        if(*c.first) (*c.first)(std::forward<A>(args)...);
                        // if it's marked as 'once': add to delete list.
                        if(c.second) to_delete.insert(c.first);
                    }
                    catch(...)
                    {
                        // TODO: handle exception
                    }
                }

                // remove 'once' callbacks
                list_.remove_if([&](std::pair<callback_ptr, bool>& it) {
                    return to_delete.find(it.first) != to_delete.end();
                });
            }

        private:
            std::list<std::pair<callback_ptr, bool>> list_;
        };

        class event_emitter
        {
        public:
            event_emitter()
                : callbacks_()
            {}
            ~event_emitter()
            {}

        public:
            template<typename T>
            void* add(const T& callback, bool once=false)
            {
                if(!callbacks_)
                {
                    callbacks_.reset(new scl<T>());
                    assert(callbacks_);
                }

                auto x = new T(callback);
                assert(x);

                callbacks_->add(x, once);

                return reinterpret_cast<void*>(x);
            }

            template<typename T>
            static void add(void* target, const T& callback, bool once=false)
            {
                auto self = reinterpret_cast<event_emitter*>(target);
                assert(self);
                self->add<T>(callback, once);
            }

            bool remove(void* ptr)
            {
                if(callbacks_) return callbacks_->remove(ptr);
                return false;
            }

            template<typename T, typename ...A>
            void invoke(A&&... args)
            {
                auto x = dynamic_cast<scl<T>*>(callbacks_.get());
                if(x)
                {
                    x->invoke(std::forward<A>(args)...);
                }
            }

            template<typename T, typename ...A>
            static void invoke(void* target, A&&... args)
            {
                auto self = reinterpret_cast<event_emitter*>(target);
                assert(self);
                self->invoke<T>(std::forward<A>(args)...);
            }

            void clear()
            {
                if(callbacks_) callbacks_->clear();
            }

            std::size_t count() const
            {
                if(callbacks_) return callbacks_->count();
                return 0;
            }

        private:
            std::shared_ptr<scl_base> callbacks_;
        };
    }
}

#endif
