#ifndef __TCP_H__
#define __TCP_H__

#include "base.h"
#include "handle.h"
#include "stream.h"
#include "net.h"
#include "callback.h"

namespace native
{
    namespace net
    {
        class tcp : public native::base::stream
        {
        public:
            template<typename X>
            tcp(X* x)
                : stream(x)
            { }

        public:
            tcp()
                : native::base::stream(new uv_tcp_t)
            {
                uv_tcp_init(uv_default_loop(), get<uv_tcp_t>());
            }

            tcp(native::loop& l)
                : native::base::stream(new uv_tcp_t)
            {
                uv_tcp_init(l.get(), get<uv_tcp_t>());
            }

            static std::shared_ptr<tcp> create()
            {
                return std::shared_ptr<tcp>(new tcp);
            }

            // TODO: bind and listen
            static std::shared_ptr<tcp> create_server(const std::string& ip, int port) {
                return nullptr;
            }

            bool nodelay(bool enable) { return uv_tcp_nodelay(get<uv_tcp_t>(), enable?1:0) == 0; }
            bool keepalive(bool enable, unsigned int delay) { return uv_tcp_keepalive(get<uv_tcp_t>(), enable?1:0, delay) == 0; }
            bool simultanious_accepts(bool enable) { return uv_tcp_simultaneous_accepts(get<uv_tcp_t>(), enable?1:0) == 0; }

            bool bind(const std::string& ip, int port) { return uv_tcp_bind(get<uv_tcp_t>(), uv_ip4_addr(ip.c_str(), port)) == 0; }
            bool bind6(const std::string& ip, int port) { return uv_tcp_bind6(get<uv_tcp_t>(), uv_ip6_addr(ip.c_str(), port)) == 0; }

            template<typename callback_t>
            bool connect(const std::string& ip, int port, callback_t callback)
            {
                callbacks::store(get()->data, native::internal::uv_cid_connect, callback);
                return uv_tcp_connect(new uv_connect_t, get<uv_tcp_t>(), to_ip4_addr(ip, port), [](uv_connect_t* req, int status) {
                    callbacks::invoke<callback_t>(req->handle->data, native::internal::uv_cid_connect, status);
                    delete req;
                }) == 0;
            }

            template<typename callback_t>
            bool connect6(const std::string& ip, int port, callback_t callback)
            {
                callbacks::store(get()->data, native::internal::uv_cid_connect6, callback);
                return uv_tcp_connect6(new uv_connect_t, get<uv_tcp_t>(), to_ip6_addr(ip, port), [](uv_connect_t* req, int status) {
                    callbacks::invoke<callback_t>(req->handle->data, native::internal::uv_cid_connect6, status);
                    delete req;
                }) == 0;
            }

            bool getsockname(bool& ip4, std::string& ip, int& port)
            {
                struct sockaddr_storage addr;
                int len = sizeof(addr);
                if(uv_tcp_getsockname(get<uv_tcp_t>(), reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
                {
                    ip4 = (addr.ss_family == AF_INET);
                    if(ip4) return from_ip4_addr(reinterpret_cast<ip4_addr*>(&addr), ip, port);
                    else return from_ip6_addr(reinterpret_cast<ip6_addr*>(&addr), ip, port);
                }
                return false;
            }

            bool getpeername(bool& ip4, std::string& ip, int& port)
            {
                struct sockaddr_storage addr;
                int len = sizeof(addr);
                if(uv_tcp_getpeername(get<uv_tcp_t>(), reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
                {
                    ip4 = (addr.ss_family == AF_INET);
                    if(ip4) return from_ip4_addr(reinterpret_cast<ip4_addr*>(&addr), ip, port);
                    else return from_ip6_addr(reinterpret_cast<ip6_addr*>(&addr), ip, port);
                }
                return false;
            }
        };
    }
}

#endif
