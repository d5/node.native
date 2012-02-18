#ifndef __NET_H__
#define __NET_H__

#include "base.h"
#include "detail.h"
#include "stream.h"
#include "error.h"
#include "process.h"
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

        class Socket : public Stream
        {
        public:
            Socket(std::shared_ptr<detail::stream> handle, Server* server, bool allow_half_open)
                : Stream(handle.get(), true, true)
                , stream_(handle)
                , server_(server)
                , allow_half_open_(allow_half_open)
            {}

            virtual ~Socket()
            {}

        private:
            std::shared_ptr<detail::stream> stream_;
            bool allow_half_open_;
            Server* server_;
        };

        class Server : public EventEmitter
        {
            friend ServerPtr createServer(ev::connection::callback_type, bool);

            Server(bool allowHalfOpen, ev::connection::callback_type callback)
                : EventEmitter()
                , stream_()
                , connections_(0)
                , max_connections_(0)
                , allow_half_open_(allowHalfOpen)
                , backlog_(128)
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

            // listen over TCP socket
            bool listen(int port, const std::string& host=std::string("0.0.0.0"), ev::listening::callback_type listeningListener=nullptr)
            {
                return listen_(host, port, listeningListener);
            }

            // listen over unix-socket
            bool listen(const std::string& path, ev::listening::callback_type listeningListener=nullptr)
            {
                return listen_(path, 0, listeningListener);
            }

        protected:
            static detail::stream* create_server_handle(const std::string& ip_or_path, int port)
            {
                detail::error e;
                if(isIPv4(ip_or_path))
                {
                    auto handle = new detail::tcp;
                    assert(handle);

                    auto err = handle->bind(ip_or_path, port);
                    if(err)
                    {
                        handle->close();
                        return nullptr;
                    }

                    return handle;
                }
                else if(isIPv6(ip_or_path))
                {
                    auto handle = new detail::tcp;
                    assert(handle);

                    auto err = handle->bind6(ip_or_path, port);
                    if(err)
                    {
                        handle->close();
                        return nullptr;
                    }

                    return handle;
                }
                else
                {
                    auto handle = new detail::pipe;
                    assert(handle);

                    auto err = handle->bind(ip_or_path);
                    if(err)
                    {
                        handle->close();
                        return nullptr;
                    }

                    return handle;
                }
            }

            bool listen_(const std::string& ip_or_path, int port, ev::listening::callback_type listeningListener)
            {
                if(listeningListener) on<ev::listening>(listeningListener);

                if(!stream_)
                {
                    stream_.reset(create_server_handle(ip_or_path, port));
                    if(!stream_)
                    {
                        process::nextTick([&](){
                            emit<ev::error>(Exception("Failed to create a server socket (1)."));
                        });
                        return false;
                    }
                }

                detail::error e = stream_->listen(backlog_, [&](std::shared_ptr<detail::stream> s, detail::error e){
                    if(e)
                    {
                        emit<ev::error>(Exception("Failed to accept client socket (1)."));
                    }
                    else
                    {
                        if(max_connections_ && connections_ > max_connections_)
                        {
                            // TODO: where is "error" event?
                            s->close(); // just close down?
                            return;
                        }

                        SocketPtr socket(new Socket(s, this, allow_half_open_));
                        assert(socket);

                        socket->resume();

                        connections_++;
                        emit<ev::connection>(socket);

                        socket->emit<ev::connect>();
                    }
                });

                if(e)
                {
                    stream_->close();
                    stream_.reset();
                    process::nextTick([&](){
                        emit<ev::error>(Exception("Failed to initiate listening on server socket (1)."));
                    });
                    return false;
                }

                process::nextTick([&](){
                    emit<ev::listening>();
                });
                return true;
            }

        public:
            std::shared_ptr<detail::stream> stream_;
            std::size_t connections_;
            std::size_t max_connections_;
            bool allow_half_open_;
            int backlog_;
        };



        ServerPtr createServer(ev::connection::callback_type callback=nullptr, bool allowHalfOpen=false)
        {
            return ServerPtr(new Server(allowHalfOpen, callback));
        }
    }
}

#endif
