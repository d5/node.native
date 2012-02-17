#ifndef __NET_H__
#define __NET_H__

#include "base.h"
#include "detail.h"
#include "stream.h"
#include <arpa/inet.h>

namespace native
{
    namespace net
    {
        class Server;
        typedef std::shared_ptr<Server> ServerPtr;

        class Socket;
        typedef std::shared_ptr<Socket> SocketPtr;

        /**
         *  @brief Gets the version of IP address format.
         *
         *  @param input    Input IP string.
         *
         *  @retval 4       Input string is a valid IPv4 address.
         *  @retval 6       Input string is a valid IPv6 address.
         *  @retval 0       Input string is not a valid IP address.
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
         *  @brief Tests if the input string is valid IPv4 address.
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

        class Server : public EventEmitter
        {
            friend ServerPtr createServer(ev::connection::callback_type, bool);

            Server(bool allowHalfOpen, ev::connection::callback_type callback)
                : EventEmitter()
                , connections_(0)
                , allow_half_open_(allowHalfOpen)
            {
                registerEvent<ev::listening>();
                registerEvent<ev::connection>();
                registerEvent<ev::close>();
                registerEvent<ev::error>();

                if(callback) on<ev::connection>(callback);
            }

        public:
            virtual ~Server()
            {}

#if 0
            // listen over TCP socket
            bool listen(int port, const std::string& host=std::string('0.0.0.0'), ev::listening::callback_type listeningListener=nullptr)
            {
                if(listeningListener) on<ev::listening>(listeningListener);

                // ....

                return false;
            }

            // listen over unix-socket
            bool listen(const std::string& path, ev::listening::callback_type listeningListener=nullptr)
            {
                return false;
            }
#endif
        public:
            std::size_t connections_;
            bool allow_half_open_;
        };

        class Socket : public Stream
        {
        public:
            Socket()
                : Stream(&tcp_, true, true)
            {}

            virtual ~Socket()
            {}
        private:
            detail::tcp tcp_;
        };

        ServerPtr createServer(ev::connection::callback_type callback=nullptr, bool allowHalfOpen=false)
        {
            return ServerPtr(new Server(allowHalfOpen, callback));
        }
    }
}

#endif
