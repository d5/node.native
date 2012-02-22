#ifndef __DETAIL_HTTP_H__
#define __DETAIL_HTTP_H__

#include "base.h"
#include "stream.h"
#include "utility.h"

namespace native
{
    namespace detail
    {
        class http_parse_result;
        bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

        class http_parse_result
        {
            friend bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

            typedef std::map<std::string, std::string, util::text::ci_less> headers_type;

        public:
            class url_obj
            {
            public:
                url_obj(const char* buffer, std::size_t length, bool is_connect=false)
                    : handle_(), buf_(buffer, length)
                {
                    int r = http_parser_parse_url(buffer, length, is_connect, &handle_);
                    if(r) throw r;
                }

                ~url_obj()
                {}

            public:
                std::string schema() const
                {
                    if(has_schema()) return buf_.substr(handle_.field_data[UF_SCHEMA].off, handle_.field_data[UF_SCHEMA].len);
                    return "HTTP";
                }

                std::string host() const
                {
                    // TODO: if not specified, use host name
                    if(has_schema()) return buf_.substr(handle_.field_data[UF_HOST].off, handle_.field_data[UF_HOST].len);
                    return std::string("localhost");
                }

                int port() const
                {
                    if(has_path()) return static_cast<int>(handle_.port);
                    return (schema() == "HTTP" ? 80 : 443);
                }

                std::string path() const
                {
                    if(has_path()) return buf_.substr(handle_.field_data[UF_PATH].off, handle_.field_data[UF_PATH].len);
                    return std::string("/");
                }

                std::string query() const
                {
                    if(has_query()) return buf_.substr(handle_.field_data[UF_QUERY].off, handle_.field_data[UF_QUERY].len);
                    return std::string();
                }

                std::string fragment() const
                {
                    if(has_query()) return buf_.substr(handle_.field_data[UF_FRAGMENT].off, handle_.field_data[UF_FRAGMENT].len);
                    return std::string();
                }

            private:
                bool has_schema() const { return handle_.field_set & (1<<UF_SCHEMA); }
                bool has_host() const { return handle_.field_set & (1<<UF_HOST); }
                bool has_port() const { return handle_.field_set & (1<<UF_PORT); }
                bool has_path() const { return handle_.field_set & (1<<UF_PATH); }
                bool has_query() const { return handle_.field_set & (1<<UF_QUERY); }
                bool has_fragment() const { return handle_.field_set & (1<<UF_FRAGMENT); }

            private:
                http_parser_url handle_;
                std::string buf_;
            };

        public:
            http_parse_result()
                : url_()
                , headers_()
            {}

            ~http_parse_result()
            {}

        public:
            const url_obj& url() const { return *url_; }
            const headers_type& headers() const { return headers_; }

        private:
            std::shared_ptr<url_obj> url_;
            headers_type headers_;
        };

        typedef std::function<void(const http_parse_result*, const char*)> http_parser_callback_type;

        class http_parser_obj
        {
            friend bool parse_http_request(stream&, http_parser_callback_type callback);

        private:
            http_parser_obj()
                : parser_()
                , was_header_value_(true)
                , last_header_field_()
                , last_header_value_()
                , settings_()
                , callback_()
            {
            }

        public:
            ~http_parser_obj()
            {
            }

        private:
            http_parser parser_;
            http_parser_settings settings_;
            bool was_header_value_;
            std::string last_header_field_;
            std::string last_header_value_;

            http_parse_result result_;

            http_parser_callback_type callback_;
        };

        /**
         *  @brief Parses HTTP request from the input stream.
         *
         *  @param input    Input stream.
         *  @param callback Callback funtion (or any compatible callbale object).
         *
         *  @remarks
         *      Callback function must have 2 parameters:
         *          - result: parsed result. If there was an error, this parameter is nullptr.
         *          - error: error message. If there was no error, this parameter is nullptr.
         *
         *      Note that this function is not non-blocking, meaning the callback function is invoked synchronously.
         */
        bool parse_http_request(stream& input, http_parser_callback_type callback)
        {
            http_parser_obj obj;
            http_parser_init(&obj.parser_, HTTP_REQUEST);
            obj.parser_.data = &obj;
            obj.callback_ = callback;

            obj.settings_.on_url = [](http_parser* parser, const char *at, size_t len) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                assert(at && len);
                try
                {
                    obj->result_.url_.reset(new http_parse_result::url_obj(at, len));
                    return 0;
                }
                catch(int e)
                {
                    if(obj->callback_) obj->callback_(nullptr, "Failed to parser URL in the request.");
                    return 1;
                }
            };
            obj.settings_.on_header_field = [](http_parser* parser, const char* at, size_t len) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                if(obj->was_header_value_)
                {
                    // new field started
                    if(!obj->last_header_field_.empty())
                    {
                        // add new entry
                        obj->result_.headers_[obj->last_header_field_] = obj->last_header_value_;
                        obj->last_header_value_.clear();
                    }

                    obj->last_header_field_ = std::string(at, len);
                    obj->was_header_value_ = false;
                }
                else
                {
                    // appending
                    obj->last_header_field_ += std::string(at, len);
                }
                return 0;
            };
            obj.settings_.on_header_value = [](http_parser* parser, const char* at, size_t len) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                if(!obj->was_header_value_)
                {
                    obj->last_header_value_ = std::string(at, len);
                    obj->was_header_value_ = true;
                }
                else
                {
                    // appending
                    obj->last_header_value_ += std::string(at, len);
                }
                return 0;
            };
            obj.settings_.on_headers_complete = [](http_parser* parser) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                // add last entry if any
                if(!obj->last_header_field_.empty())
                {
                    // add new entry
                    obj->result_.headers_[obj->last_header_field_] = obj->last_header_value_;
                }

                // TODO: implement body contents parsing
                // return 0;
                return 1; // do not parse body
            };
            obj.settings_.on_message_complete = [](http_parser* parser) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                // invoke callback
                if(obj->callback_) obj->callback_(&obj->result_, nullptr);
                return 0;
            };

            return false;
        }

        std::string http_status_text(int status_code)
        {
            switch(status_code)
            {
                case 100: return "Continue";
                case 101: return "Switching Protocols";
                case 102: return "Processing";                 // RFC 2518; obsoleted by RFC 4918
                case 200: return "OK";
                case 201: return "Created";
                case 202: return "Accepted";
                case 203: return "Non-Authoritative Information";
                case 204: return "No Content";
                case 205: return "Reset Content";
                case 206: return "Partial Content";
                case 207: return "Multi-Status";               // RFC 4918
                case 300: return "Multiple Choices";
                case 301: return "Moved Permanently";
                case 302: return "Moved Temporarily";
                case 303: return "See Other";
                case 304: return "Not Modified";
                case 305: return "Use Proxy";
                case 307: return "Temporary Redirect";
                case 400: return "Bad Request";
                case 401: return "Unauthorized";
                case 402: return "Payment Required";
                case 403: return "Forbidden";
                case 404: return "Not Found";
                case 405: return "Method Not Allowed";
                case 406: return "Not Acceptable";
                case 407: return "Proxy Authentication Required";
                case 408: return "Request Time-out";
                case 409: return "Conflict";
                case 410: return "Gone";
                case 411: return "Length Required";
                case 412: return "Precondition Failed";
                case 413: return "Request Entity Too Large";
                case 414: return "Request-URI Too Large";
                case 415: return "Unsupported Media Type";
                case 416: return "Requested Range Not Satisfiable";
                case 417: return "Expectation Failed";
                case 418: return "I\"m a teapot";              // RFC 2324
                case 422: return "Unprocessable Entity";       // RFC 4918
                case 423: return "Locked";                     // RFC 4918
                case 424: return "Failed Dependency";          // RFC 4918
                case 425: return "Unordered Collection";       // RFC 4918
                case 426: return "Upgrade Required";           // RFC 2817
                case 500: return "Internal Server Error";
                case 501: return "Not Implemented";
                case 502: return "Bad Gateway";
                case 503: return "Service Unavailable";
                case 504: return "Gateway Time-out";
                case 505: return "HTTP Version not supported";
                case 506: return "Variant Also Negotiates";    // RFC 2295
                case 507: return "Insufficient Storage";       // RFC 4918
                case 509: return "Bandwidth Limit Exceeded";
                case 510: return "Not Extended";                // RFC 2774
                default: assert(false); return std::string();
            }
        }
    }
}

#endif
