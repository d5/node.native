#ifndef __HTTP_H__
#define __HTTP_H__

#include <sstream>
#include "base.h"
#include "detail.h"
#include "events.h"
#include "net.h"

namespace native
{
    // forward declarations
    namespace http
    {
        class ServerRequest;
        class ServerResponse;
    }

    namespace event
    {
        struct request : public util::callback_def<http::ServerRequest*, http::ServerResponse*> {};
        struct checkContinue : public util::callback_def<http::ServerRequest*, http::ServerResponse*> {};
        struct upgrade : public util::callback_def<http::ServerRequest*, net::Socket*, const Buffer&> {};
    }

    namespace http
    {
        class ServerRequest : public Stream
        {
        public:
            ServerRequest(detail::stream* stream)
                : Stream(stream, true, false)
            {
                registerEvent<event::data>();
                registerEvent<event::end>();
                registerEvent<event::close>();
            }

            virtual ~ServerRequest()
            {}

        public:
            std::string method() const { return ""; }
            std::string url() const { return ""; }
            const util::dict& headers() const;
            const util::dict& trailers() const; // only populated after 'end' event
            std::string httpVersion() const { return ""; }

            //void setEncoding(const std::string& encoding);
            //void pause();
            //void resume();

            net::Socket* connection() { return nullptr; }
        };

        class ServerResponse : public Stream
        {
        public:
            ServerResponse(detail::stream* stream)
                : Stream(stream, false, true)
            {
                registerEvent<event::close>();
            }

            virtual ~ServerResponse()
            {}

        public:
            void writeContinue() {}
            void writeHead(int statusCode, const std::string& reasonPhrase, const util::idict& headers) {}

            int statusCode() const { return 0; }
            void statusCode(int value) {} // calling this after response was sent is error

            void setHeader(const std::string& name, const std::string& value) {}
            std::string getHeader(const std::string& name, const std::string& default_value=std::string()) { return default_value; }
            bool getHeader(const std::string& name, std::string& value) { return false; }
            bool removeHeader(const std::string& name);

            //void write(const Buffer& data) {}
            void addTrailers(const util::dict& headers) {}
            //void end(const Buffer& data) {}
        };

        class Server : public net::Server
        {
        public:
            Server()
                : net::Server()
            {
                registerEvent<event::request>();
                registerEvent<event::connection>();
                registerEvent<event::close>();
                registerEvent<event::checkContinue>();
                registerEvent<event::upgrade>();
            }

            virtual ~Server()
            {}

        public:
            //void close();
            //void listen(port, hostname, callback);
            //void listen(path, callback);
        };

        Server* createServer(std::function<void(Server*)> callback)
        {
            return nullptr;
        }
    }
}

#endif
