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
                , trailers_()
            {
                assert(socket_);

                registerEvent<event::data>();
                registerEvent<event::end>();
                registerEvent<event::close>();

                socket_->on<event::data>([this](const Buffer& buffer){ emit<event::data>(buffer); });
                socket_->on<event::end>([this](){ emit<event::end>(); });
                socket_->on<event::close>([this](){ emit<event::close>(); });
            }

            virtual ~ServerRequest()
            {}

        public:
            const std::string& schema() const { return req_info_.schema(); }
            const std::string& host() const { return req_info_.host(); }
            int port() const { return req_info_.port(); }
            const std::string& path() const { return req_info_.path(); }
            const std::string& query() const { return req_info_.query(); }
            const std::string& fragment() const { return req_info_.fragment(); }
            const headers_type& headers() const { return req_info_.headers(); }
            const std::string& method() const { return req_info_.method(); }
            const std::string& http_version() const { return req_info_.http_version(); }
            bool upgrade() const { return req_info_.upgrade(); }

            const headers_type& trailers() const { return trailers_; } // only populated after 'end' event

            //void setEncoding(const std::string& encoding);
            //void pause();
            //void resume();

            net::Socket* connection() { return nullptr; }

        private:
            net::Socket* socket_;
            detail::http_parse_result req_info_;
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
                assert(socket_);

                registerEvent<event::close>();
                registerEvent<event::error>();

                socket_->on<event::close>([this](){ emit<event::close>(); });
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

            void write(const Buffer& data)
            {
                socket_->write(data);
            }

            void addTrailers(const headers_type& headers) {}

            void end(const Buffer& data)
            {
                assert(socket_);

                if(socket_->end(data))
                {
                    socket_ = nullptr;
                }
                else
                {
                    emit<event::error>(Exception("Failed to close the socket."));
                }
            }

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

                            auto server_req = new ServerRequest(socket, parse_result);
                            assert(server_req);

                            auto server_res = new ServerResponse(socket);
                            assert(server_res);

                            emit<event::request>(server_req, server_res);
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
