#ifndef __DEV_H__
#define __DEV_H__

#include "base.h"
#include "events.h"

namespace dev
{
    class Buffer {};

    class Exception {};

    template<typename ...E>
    class Stream : public EventEmitter<E...> {};

    namespace net
    {
        class Socket : public Stream<>
        {
        };

        typedef std::tuple<
            ev::listening, std::function<void()>,
            ev::connection, std::function<void(Socket&)>,
            ev::error, std::function<void(Exception&)>,
            ev::close, std::function<void()>
        > server_events;

        template<typename ...E>
        class Server : public EventEmitter<server_events, E...>
        {
        public:
            typedef std::function<void()> ListeningListener;
        };
    }

    namespace http
    {
        typedef std::tuple<
            ev::data, std::function<void(const std::string&)>,
            ev::end, std::function<void()>,
            ev::close, std::function<void()>
        > server_req_events;

        class ServerRequest : public EventEmitter<server_req_events>
        {
        public:
            typedef std::function<void(const std::string&)> DataListener;
            typedef std::function<void()> EndListener;
            typedef std::function<void()> CloseListener;
        };

        class ServerResponse {};

        typedef std::tuple<
            ev::request, std::function<void(ServerRequest&, ServerResponse&)>,
            ev::connection, std::function<void(net::Socket&)>,
            ev::close, std::function<void()>,
            ev::checkContinue, std::function<void(ServerRequest&, ServerResponse&)>,
            ev::upgrade, std::function<void(ServerRequest&, net::Socket&, Buffer&)>,
            ev::clientError, std::function<void(Exception&)>
        > server_events;

        class Server : public net::Server<server_events>
        {
        protected:
            Server(){}

        public:
            virtual ~Server() {}

        public:
            static std::shared_ptr<Server> createServer(callback_type<ev::request>::type requestListener)
            {
                return nullptr;
            }

            bool listen(int port, const std::string& hostname, callback_type<ev::listening>::type callback)
            {
                return false;
            }

            bool listen(int port, callback_type<ev::listening>::type callback)
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
}

#endif
