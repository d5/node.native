#ifndef __NET_H__
#define __NET_H__

#include "base.h"
#include <arpa/inet.h>
#include "events.h"
#include "stream.h"
#include "callback.h"

namespace native
{
    namespace detail
    {
        typedef sockaddr_in ip4_addr;
        typedef sockaddr_in6 ip6_addr;

        ip4_addr to_ip4_addr(const std::string& ip, int port) { return uv_ip4_addr(ip.c_str(), port); }
        ip6_addr to_ip6_addr(const std::string& ip, int port) { return uv_ip6_addr(ip.c_str(), port); }

        bool from_ip4_addr(ip4_addr* src, std::string& ip, int& port)
        {
            char dest[16];
            if(uv_ip4_name(src, dest, 16) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin_port));
                return true;
            }
            return false;
        }

        bool from_ip6_addr(ip6_addr* src, std::string& ip, int& port)
        {
            char dest[46];
            if(uv_ip6_name(src, dest, 46) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin6_port));
                return true;
            }
            return false;
        }

        class tcp : public stream
        {
        public:
            template<typename X>
            tcp(X* x)
                : stream(x)
            { }

        public:
            tcp()
                : stream(new uv_tcp_t)
            {
                uv_tcp_init(uv_default_loop(), get<uv_tcp_t>());
            }

            virtual ~tcp()
            {
            }


            bool nodelay(bool enable) { return uv_tcp_nodelay(get<uv_tcp_t>(), enable?1:0) == 0; }
            bool keepalive(bool enable, unsigned int delay) { return uv_tcp_keepalive(get<uv_tcp_t>(), enable?1:0, delay) == 0; }
            bool simultanious_accepts(bool enable) { return uv_tcp_simultaneous_accepts(get<uv_tcp_t>(), enable?1:0) == 0; }

            bool bind(const std::string& ip, int port) { return uv_tcp_bind(get<uv_tcp_t>(), uv_ip4_addr(ip.c_str(), port)) == 0; }
            bool bind6(const std::string& ip, int port) { return uv_tcp_bind6(get<uv_tcp_t>(), uv_ip6_addr(ip.c_str(), port)) == 0; }

            bool connect(const std::string& ip, int port, std::function<void(Error)> callback)
            {
                callbacks::store(get()->data, uv_cid_connect, callback);
                return uv_tcp_connect(new uv_connect_t, get<uv_tcp_t>(), to_ip4_addr(ip, port), [](uv_connect_t* req, int status) {
                    callbacks::invoke<decltype(callback)>(req->handle->data, uv_cid_connect, Error::fromStatusCode(status));
                    delete req;
                }) == 0;
            }

            bool connect6(const std::string& ip, int port, std::function<void(Error)> callback)
            {
                callbacks::store(get()->data, uv_cid_connect6, callback);
                return uv_tcp_connect6(new uv_connect_t, get<uv_tcp_t>(), to_ip6_addr(ip, port), [](uv_connect_t* req, int status) {
                    callbacks::invoke<decltype(callback)>(req->handle->data, uv_cid_connect6, Error::fromStatusCode(status));
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

    namespace net
    {
        // IP address versions
        static constexpr int IPv4 = 4;
        static constexpr int IPv6 = 6;


        /**
         *  @brief Gets the version of IP address format.
         *
         *  @param input    Input IP string.
         *
         *  @retval 4(native::net::IPv4)       Input string is a valid IPv4 address.
         *  @retval 6(native::net::IPv6)       Input string is a valid IPv6 address.
         *  @retval 0                          Input string is not a valid IP address.
         */
        int isIP(const std::string& input)
        {
            if(input.empty()) return 0;

            unsigned char buf[sizeof(in6_addr)];

            if(inet_pton(AF_INET, input.c_str(), buf) == 1) return 4;
            else if(inet_pton(AF_INET6, input.c_str(), buf) == 1) return 6;

            return 0;
        }

        /**
         *  @brief Test if the input string is valid IPv4 address.
         *
         *  @param input    Input IP string.
         *
         *  @retval true    Input string is a valid IPv4 address.
         *  @retval false    Input string is not a valid IPv4 address.
         */
        bool isIPv4(const std::string& input) { return isIP(input) == 4; }

        /**
         *  @brief Test if the input string is valid IPv6 address.
         *
         *  @param input    Input IP string.
         *
         *  @retval true    Input string is a valid IPv6 address.
         *  @retval false    Input string is not a valid IPv6 address.
         */
        bool isIPv6(const std::string& input) { return isIP(input) == 6; }

#if 0
        template<typename ...E>
        class SocketT;

        typedef SocketT<> Socket;

        typedef std::tuple<
            native::ev::connect, std::function<void()>,
            native::ev::timeout, std::function<void()>
        > socket_events;

        template<typename ...E>
        class SocketT : public StreamT<true, true, socket_events, E...>
        {
            typedef StreamT<true, true, socket_events, E...> base_type;

        public:
            SocketT()
                : base_type(reinterpret_cast<uv_stream_t*>(&handle_))
            {}

            virtual ~SocketT()
            {}

        public:
            bool connect(int port, const std::string& host="127.0.0.1", std::function<void()> connectListener=nullptr)
            {
                if(destroyed_ || !handle_)
                {
                    // create an underlying handle
                    if(path_.empty())
                    {
                        // create TCP socket
                    }
                    else
                    {
                        // create unix-socket

                    }
                }

                if(handle_->connect(host, port, [this](Error e){
                    connecting_ = false;

                    if(e)
                    {
                        this->template emit<native::ev::error>(e);
                    }
                    else
                    {
                        this->template emit<native::ev::connect>();
                    }
                }))
                {
                    connecting_ = true;

                    if(connectListener)
                    {
                        this->template addListener<native::ev::connect>(connectListener);
                    }

                    return true;
                }

                return false;
            }

        private:
            virtual bool write_(const std::string& str, const std::string& encoding, std::function<void()> callback)
            {
                return false;
            }

        private:
            uv_tcp_t handle_;
            bool connecting_;
            bool destroyed_;
            std::string path_;
        };
#endif
    }


}

#endif
