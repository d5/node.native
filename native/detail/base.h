#ifndef __DETAIL_BASE_H__
#define __DETAIL_BASE_H__

#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include <map>
#include <algorithm>
#include <list>
#include <set>
#include <tuple>
#include <arpa/inet.h>
#include <uv.h>
#include <http_parser.h>

namespace native
{
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
            resval() : err_() {} // default: no error
            explicit resval(uv_err_t e) : err_(e) {}
            explicit resval(uv_err_code code) : err_ {code, 0} {}
            explicit resval(std::nullptr_t) : err_() {}
            resval(const resval& c) : err_(c.err_) {}
            resval(resval&& c) : err_(std::move(c.err_)) {}
            ~resval() {}

            operator bool() { return err_.code == UV_OK; }

            uv_err_code code() const { return err_.code; }
            const char* name() const { return uv_err_name(err_); }
            const char* str() const { return uv_strerror(err_); }
        private:
            uv_err_t err_;
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

        class sigslot_base
        {
        public:
            sigslot_base() {}
            virtual ~sigslot_base() {}
        public:
            virtual void add_callback(void*, bool once=false) = 0;
            virtual bool remove_callback(void*) = 0;
            virtual void reset() = 0;
            virtual std::size_t callback_count() const = 0;
        };

        template<typename>
        class sigslot;

        template<typename R, typename ...P>
        class sigslot<std::function<R(P...)>> : public sigslot_base
        {
        public:
            typedef std::function<R(P...)> callback_type;
            typedef std::shared_ptr<callback_type> callback_ptr;

        public:
            sigslot()
                : callbacks_()
            {
            }
            virtual ~sigslot()
            {
            }

        public:
            virtual void add_callback(void* callback, bool once=false)
            {
                assert(callback);

                // wrap the callback object with shared_ptr<>.
                callbacks_.push_back(std::make_pair(callback_ptr(reinterpret_cast<callback_type*>(callback)), once));
            }

            virtual bool remove_callback(void* callback)
            {
                // find the matching callback
                auto d = callbacks_.end();
                for(auto it=callbacks_.begin();it!=callbacks_.end();++it)
                {
                    if(reinterpret_cast<void*>(it->first.get()) == callback)
                    {
                        d = it;
                        break;
                    }
                }

                // if found: delete it from the list.
                if(d != callbacks_.end())
                {
                    callbacks_.erase(d);
                    return true;
                }

                // failed to find the callback
                return false;
            }

            virtual void reset()
            {
                // delete all callbacks
                callbacks_.clear();
            }

            virtual std::size_t callback_count() const
            {
                // the number of callbacks
                return callbacks_.size();
            }

            template<typename ...A>
            void invoke(A&&... args)
            {
                // set of callbacks to delete after execution.
                std::set<callback_ptr> to_delete;

                // copy callback list: to avoid modification inside the 'for' loop.
                auto callbacks_copy = callbacks_;
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
                callbacks_.remove_if([&](std::pair<callback_ptr, bool>& it) {
                    return to_delete.find(it.first) != to_delete.end();
                });
            }

        private:
            std::list<std::pair<callback_ptr, bool>> callbacks_;
        };

        typedef std::shared_ptr<sigslot_base> callback_object_ptr;

        class callbacks
        {
        public:
            callbacks(int max_callbacks)
                : lut_(max_callbacks)
            {
            }
            ~callbacks()
            {
            }

            template<typename ...A>
            static void store(void* target, int cid, std::function<void(A...)> callback)
            {
                auto self = reinterpret_cast<callbacks*>(target);
                assert(self);

                auto x = new sigslot<decltype(callback)>();

                self->lut_[cid].reset(x);
                self->lut_[cid]->add_callback(new decltype(callback)(callback));
            }

            template<typename F, typename ...A>
            static void invoke(void* target, int cid, A&&... args)
            {
                auto self = reinterpret_cast<callbacks*>(target);
                assert(self);

                auto f = self->lut_[cid].get();
                auto x = dynamic_cast<sigslot<F>*>(self->lut_[cid].get());
                assert(x);

                x->invoke(std::forward<A>(args)...);
            }

        private:
            std::vector<callback_object_ptr> lut_;
        };
    }
}

#endif
