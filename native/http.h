#ifndef __HTTP_H__
#define __HTTP_H__

#include <sstream>
#include "base.h"
#include "detail.h"
#include "events.h"
#include "net.h"
#include "process.h"

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
        class Server;

        typedef std::map<std::string, std::string, util::text::ci_less> headers_type;

        class ServerRequest : public EventEmitter
        {
            friend class Server;

        protected:
            ServerRequest(net::Socket* socket, const detail::http_parse_result* parse_result)
                : EventEmitter()
                , socket_(socket)
                , req_info_(*parse_result)
                , headers_()
                , trailers_()
            {
                assert(socket_);

                registerEvent<event::data>();
                registerEvent<event::end>();
                registerEvent<event::close>();
            }

            virtual ~ServerRequest()
            {}

        public:
            std::string method() const { return ""; }
            std::string url() const { return ""; }
            const headers_type& headers() const { return headers_; }
            const headers_type& trailers() const { return trailers_; } // only populated after 'end' event
            std::string httpVersion() const { return ""; }

            //void setEncoding(const std::string& encoding);
            //void pause();
            //void resume();

            net::Socket* connection() { return nullptr; }

        private:
            net::Socket* socket_;
            detail::http_parse_result req_info_;
            headers_type headers_;
            headers_type trailers_;
        };

        class ServerResponse : public EventEmitter
        {
            friend class Server;

        protected:
            ServerResponse(net::Socket* socket)
                : EventEmitter()
                , socket_(socket)
                , headers_()
            {
                registerEvent<event::close>();
            }

            virtual ~ServerResponse()
            {}

        public:
            void writeContinue() {}
            void writeHead(int statusCode, const std::string& reasonPhrase, const headers_type& headers) {}

            int statusCode() const { return 0; }
            void statusCode(int value) {} // calling this after response was sent is error

            void setHeader(const std::string& name, const std::string& value) {}
            std::string getHeader(const std::string& name, const std::string& default_value=std::string()) { return default_value; }
            bool getHeader(const std::string& name, std::string& value) { return false; }
            bool removeHeader(const std::string& name) { return false; }

            //void write(const Buffer& data) {}
            void addTrailers(const headers_type& headers) {}
            //void end(const Buffer& data) {}

        private:
            net::Socket* socket_;
            headers_type headers_;
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
                registerEvent<event::error>();

                on<event::connection>([&](net::Socket* socket){
                    assert(socket);

                    detail::parse_http_request(socket->stream(), [=](const detail::http_parse_result* parse_result, detail::resval rv){
                        if(!rv)
                        {
                            emit<event::error>(rv);
                        }
                        else
                        {
                            assert(parse_result);

                            printf("schema: %s\n", parse_result->schema().c_str());
                            printf("host: %s\n", parse_result->host().c_str());
                            printf("port: %d\n", parse_result->port());
                            printf("path: %s\n", parse_result->path().c_str());
                            printf("method: %s\n", parse_result->method().c_str());
                            printf("version: %s\n", parse_result->http_version().c_str());

                            for(auto x : parse_result->headers())
                            {
                                printf("%s: %s\n", x.first.c_str(), x.second.c_str());
                            }

                            //auto server_req = new ServerRequest(socket, parse_result);
                            //assert(server_req);

                            //auto server_res = new ServerResponse(socket);
                            //assert(server_res);

                            //emit<event::request>(server_req, server_req);
                        }
                    });
                });
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
            auto server = new Server();

            assert(callback);
            process::nextTick([=](){ callback(server); });

            return server;
        }
    }
}

#endif
