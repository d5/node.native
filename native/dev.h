#ifndef __DEV_H__
#define __DEV_H__

#include "base.h"
#include "events.h"

namespace dev
{
#if 0
    class Buffer {};

    class Exception {};

    class Stream : public EventEmitter {};

    namespace net
    {
        class Socket : public Stream
        {
        };

        class Server : public EventEmitter
        {
        public:
            typedef std::function<void()> ListeningListener;
        };
    }

    namespace http
    {
        class ServerRequest : public dev::EventEmitter
        {
        public:
            typedef std::function<void(const std::string&)> DataListener;
            typedef std::function<void()> EndListener;
            typedef std::function<void()> CloseListener;
        };

        class ServerResponse {};

        class Server : public net::Server
        {
        protected:
            Server(){}

        public:
            virtual ~Server() {}

        public:
            typedef std::function<void(ServerRequest&, ServerResponse&)> RequestListener;
            typedef std::function<void(net::Socket&)> ConnectionListener;
            typedef std::function<void()> CloseListener;
            typedef std::function<void(ServerRequest&, ServerResponse&)> CheckContinueListener;
            typedef std::function<void(ServerRequest&, net::Socket&, Buffer&)> UpgradeListener;
            typedef std::function<void(Exception&)> ClientErrorListener;

            static std::shared_ptr<Server> createServer(RequestListener requestListener)
            {
                return nullptr;
            }

            bool listen(int port, const std::string& hostname, net::Server::ListeningListener callback)
            {
                return false;
            }

            bool listen(int port, net::Server::ListeningListener callback)
            {
                return false;
            }

            bool close()
            {
                return false;
            }
        private:
        };
    }

    template<> struct EventEmitter::callback_type<http::Server, event::request> { typedef http::Server::RequestListener type; };
    template<> struct EventEmitter::callback_type<http::Server, event::connection> { typedef http::Server::ConnectionListener type; };
    template<> struct EventEmitter::callback_type<http::Server, event::close> { typedef http::Server::CloseListener type; };
    template<> struct EventEmitter::callback_type<http::Server, event::checkContinue> { typedef http::Server::CheckContinueListener type; };
    template<> struct EventEmitter::callback_type<http::Server, event::upgrade> { typedef http::Server::UpgradeListener type; };
    template<> struct EventEmitter::callback_type<http::Server, event::clientError> { typedef http::Server::ClientErrorListener type; };
#endif
}

#endif
