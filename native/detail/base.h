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

        class error
        {
        public:
            error() : err_() {} // default: no error
            error(uv_err_t e) : err_(e) {}
            error(uv_err_code code) : err_ {code, 0} {}
            ~error() {}

            operator bool() { return err_.code != UV_OK; }

            uv_err_code code() const { return err_.code; }
            const char* name() const { return uv_err_name(err_); }
            const char* str() const { return uv_strerror(err_); }
        private:
            uv_err_t err_;
        };

        error get_last_error() { return error(uv_last_error(uv_default_loop())); }

        sockaddr_in to_ip4_addr(const std::string& ip, int port) { return uv_ip4_addr(ip.c_str(), port); }
        sockaddr_in6 to_ip6_addr(const std::string& ip, int port) { return uv_ip6_addr(ip.c_str(), port); }

        error from_ip4_addr(sockaddr_in* src, std::string& ip, int& port)
        {
            char dest[16];
            if(uv_ip4_name(src, dest, 16) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin_port));
                return error();
            }
            return get_last_error();
        }

        error from_ip6_addr(sockaddr_in6* src, std::string& ip, int& port)
        {
            char dest[46];
            if(uv_ip6_name(src, dest, 46) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin6_port));
                return error();
            }
            return get_last_error();
        }

        class callback_object_base
        {
        public:
            callback_object_base(void* data)
                : data_(data)
            {
            }
            virtual ~callback_object_base()
            {
            }

            void* get_data() { return data_; }

        private:
            void* data_;
        };

        template<typename callback_t>
        class callback_object : public callback_object_base
        {
        public:
            callback_object(const callback_t& callback, void* data=nullptr)
                : callback_object_base(data)
                , callback_(callback)
            {
            }

            virtual ~callback_object()
            {
            }

        public:
            template<typename ...A>
            typename std::result_of<callback_t(A...)>::type invoke(A&& ... args)
            {
                if(callback_)
                {
                    // TODO: isolate an exception caught in the callback.
                    return callback_(std::forward<A>(args)...);
                }
            }

        private:
            callback_t callback_;
        };

        typedef std::shared_ptr<callback_object_base> callback_object_ptr;

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

            template<typename callback_t>
            static void store(void* target, int cid, const callback_t& callback, void* data=nullptr)
            {
                reinterpret_cast<callbacks*>(target)->lut_[cid] = callback_object_ptr(new callback_object<callback_t>(callback, data));
            }

            template<typename callback_t>
            static void* get_data(void* target, int cid)
            {
                return reinterpret_cast<callbacks*>(target)->lut_[cid]->get_data();
            }

            template<typename callback_t, typename ...A>
            static typename std::result_of<callback_t(A...)>::type invoke(void* target, int cid, A&& ... args)
            {
                auto x = dynamic_cast<callback_object<callback_t>*>(reinterpret_cast<callbacks*>(target)->lut_[cid].get());
                assert(x);
                return x->invoke(std::forward<A>(args)...);
            }

        private:
            std::vector<callback_object_ptr> lut_;
        };

        class object
        {
        public:
           object(int max_cid)
               : lut_(new callbacks(max_cid))
           {}

           virtual ~object()
           {
               // TODO: make sure that lut_ is deleted properly and exactly once.
               if(lut_)
               {
                   delete lut_;
                   lut_ = nullptr;
               }
           }

        protected:
           callbacks* lut() { return lut_; }

        private:
           callbacks* lut_;
        };

        class sigslot_base
        {
        public:
            sigslot_base() {}
            virtual ~sigslot_base() {}
        public:
            virtual void add_callback(void*) = 0;
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
            {}
            virtual ~sigslot()
            {}

        public:
            virtual void add_callback(void* callback)
            {
                callbacks_.insert(callback_ptr(reinterpret_cast<callback_type*>(callback)));
            }

            virtual bool remove_callback(void* callback)
            {
                return callbacks_.erase(callback_ptr(reinterpret_cast<callback_type*>(callback))) > 0;
            }

            virtual void reset()
            {
                callbacks_.clear();
            }

            virtual std::size_t callback_count() const
            {
                return callbacks_.size();
            }

            template<typename ...A>
            void invoke(A&&... args)
            {
                for(auto c : callbacks_)
                {
                    if(*c) (*c)(std::forward<A>(args)...);
                }
            }

        private:
            std::set<callback_ptr> callbacks_;
        };
    }
}

#endif
