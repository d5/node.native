#ifndef __HTTP_H__
#define __HTTP_H__

#include "base.h"
#include "detail.h"
#include <sstream>

namespace native
{
    namespace detail
    {
        class client_context;
        typedef std::shared_ptr<client_context> http_client_ptr;

        class response
        {
            friend class client_context;

        private:
            response(client_context* client, native::detail::tcp* socket)
                : client_(client)
                , socket_(socket)
                , headers_()
                , status_(200)
            {
                headers_["Content-Type"] = "text/html";
            }

            ~response()
            {}

        public:
            bool end(const std::string& body)
            {
                // Content-Length
                if(headers_.find("Content-Length") == headers_.end())
                {
                    std::stringstream ss;
                    ss << body.length();
                    headers_["Content-Length"] = ss.str();
                }

                std::stringstream response_text;
                response_text << "HTTP/1.1 ";
                response_text << status_ << " " << get_status_text(status_) << "\r\n";
                for(auto h : headers_)
                {
                    response_text << h.first << ": " << h.second << "\r\n";
                }
                response_text << "\r\n";
                response_text << body;

                auto str = response_text.str();
                return socket_->write(str.c_str(), static_cast<int>(str.length()), [=](Error e) {
                    if(e)
                    {
                        // TODO: handle error
                    }
                    // clean up
                    client_.reset();
                });
            }

            void set_status(int status_code)
            {
                status_ = status_code;
            }

            void set_header(const std::string& key, const std::string& value)
            {
                headers_[key] = value;
            }

            static std::string get_status_text(int status)
            {
                switch(status)
                {
                case 100: return "Continue";
                case 101: return "Switching Protocols";
                case 200: return "OK";
                case 201: return "Created";
                case 202: return "Accepted";
                case 203: return "Non-Authoritative Information";
                case 204: return "No Content";
                case 205: return "Reset Content";
                case 206: return "Partial Content";
                case 300: return "Multiple Choices";
                case 301: return "Moved Permanently";
                case 302: return "Found";
                case 303: return "See Other";
                case 304: return "Not Modified";
                case 305: return "Use Proxy";
                //case 306: return "(reserved)";
                case 307: return "Temporary Redirect";
                case 400: return "Bad Request";
                case 401: return "Unauthorized";
                case 402: return "Payment Required";
                case 403: return "Forbidden";
                case 404: return "Not Found";
                case 405: return "Method Not Allowed";
                case 406: return "Not Acceptable";
                case 407: return "Proxy Authentication Required";
                case 408: return "Request Timeout";
                case 409: return "Conflict";
                case 410: return "Gone";
                case 411: return "Length Required";
                case 412: return "Precondition Failed";
                case 413: return "Request Entity Too Large";
                case 414: return "Request-URI Too Long";
                case 415: return "Unsupported Media Type";
                case 416: return "Requested Range Not Satisfiable";
                case 417: return "Expectation Failed";
                case 500: return "Internal Server Error";
                case 501: return "Not Implemented";
                case 502: return "Bad Gateway";
                case 503: return "Service Unavailable";
                case 504: return "Gateway Timeout";
                case 505: return "HTTP Version Not Supported";
                default: assert(false);
                }
            }

        private:
            http_client_ptr client_;
            native::detail::tcp* socket_;
            std::map<std::string, std::string, native::text::ci_less> headers_;
            int status_;
        };

        class request
        {
            friend class client_context;

        private:
            request()
                : url_()
                , headers_()
            {
            }

            ~request()
            {
                //printf("~request() %x\n", this);
            }

        public:
            const url_obj& url() const { return url_; }

            const std::string& get_header(const std::string& key, const std::string& default_value=std::string()) const
            {
                auto it = headers_.find(key);
                if(it != headers_.end()) return it->second;
                return default_value;
            }

            bool get_header(const std::string& key, std::string& value) const
            {
                auto it = headers_.find(key);
                if(it != headers_.end())
                {
                    value = it->second;
                    return true;
                }
                return false;
            }

        private:
            url_obj url_;
            std::map<std::string, std::string, native::text::ci_less> headers_;
        };

        class http
        {
        public:
            http()
                : socket_(new native::detail::tcp)
            {
            }

            virtual ~http()
            {
                if(socket_)
                {
                    socket_->close([](){});
                }
            }

        public:
            static std::shared_ptr<http> create_server(const std::string& ip, int port, std::function<void(request&, response&)> callback)
            {
                auto server = std::shared_ptr<http>(new http);
                if(server->listen(ip, port, callback)) return server;
                return nullptr;
            }

            bool listen(const std::string& ip, int port, std::function<void(request&, response&)> callback)
            {
                if(!socket_->bind(ip, port)) return false;

                if(!socket_->listen([=](Error e) {
                    if(e)
                    {
                        // TODO: handle client connection error
                    }
                    else
                    {
                        auto client = new client_context(socket_.get());
                        client->parse(callback);
                    }
                })) return false;

                return true;
            }

        private:
            std::shared_ptr<native::detail::tcp> socket_;
        };
    }
}

#endif
