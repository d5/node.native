#ifndef __DETAIL_HTTP_H__
#define __DETAIL_HTTP_H__

#include <sstream>
#include "base.h"
#include "stream.h"
#include "utility.h"

namespace native
{
    namespace detail
    {

        class http_parse_result;
        typedef std::function<void(const http_parse_result*, resval)> http_parse_callback_type;

        class http_parser_context;

        class url_obj
        {
        public:
            url_obj()
                : handle_()
                , buf_()
            {}

            ~url_obj()
            {}

            bool parse(const char* buffer, std::size_t length, bool is_connect=false)
            {
                buf_ = std::string(buffer, length);
                return http_parser_parse_url(buffer, length, is_connect, &handle_) == 0;
            }

        public:
            std::string schema() const
            {
                if(has_schema()) return buf_.substr(handle_.field_data[UF_SCHEMA].off, handle_.field_data[UF_SCHEMA].len);
                return std::string();
            }

            std::string host() const
            {
                // TODO: if not specified, use host name
                if(has_schema()) return buf_.substr(handle_.field_data[UF_HOST].off, handle_.field_data[UF_HOST].len);
                return std::string();
            }

            int port() const
            {
                if(has_path()) return static_cast<int>(handle_.port);
                return 0;
            }

            std::string path() const
            {
                if(has_path()) return buf_.substr(handle_.field_data[UF_PATH].off, handle_.field_data[UF_PATH].len);
                return std::string();
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

        class http_parse_result
        {
            friend class http_parser_context;

            typedef std::map<std::string, std::string, util::text::ci_less> headers_type;

        public:
            http_parse_result()
                : headers_()
                , schema_()
                , host_()
                , port_(0)
                , path_()
                , query_()
                , fragment_()
                , method_()
                , http_version_()
                , upgrade_(false)
            {}

            http_parse_result(const http_parse_result& c)
                : headers_(c.headers_)
                , schema_(c.schema_)
                , host_(c.host_)
                , port_(c.port_)
                , path_(c.path_)
                , query_(c.query_)
                , fragment_(c.fragment_)
                , method_(c.method_)
                , http_version_(c.http_version_)
                , upgrade_(c.upgrade_)
            {}

            http_parse_result(http_parse_result&& c)
                : headers_(std::move(c.headers_))
                , schema_(std::move(c.schema_))
                , host_(std::move(c.host_))
                , port_(c.port_)
                , path_(std::move(c.path_))
                , query_(std::move(c.query_))
                , fragment_(std::move(c.fragment_))
                , method_(std::move(c.method_))
                , http_version_(std::move(c.http_version_))
                , upgrade_(c.upgrade_)
            {}

            ~http_parse_result()
            {}

        public:
            const std::string& schema() const { return schema_; }
            const std::string& host() const { return host_; }
            int port() const { return port_; }
            const std::string& path() const { return path_; }
            const std::string& query() const { return query_; }
            const std::string& fragment() const { return fragment_; }
            const headers_type& headers() const { return headers_; }
            const std::string& method() const { return method_; }
            const std::string& http_version() const { return http_version_; }
            bool upgrade() const { return upgrade_; }

        private:
            std::string schema_;
            std::string host_;
            int port_;
            std::string path_;
            std::string query_;
            std::string fragment_;
            std::string method_;
            std::string http_version_;
            bool upgrade_;
            headers_type headers_;
        };

        class http_parser_context
        {
        public:
            http_parser_context(http_parser_type parser_type)
                : parser_()
                , was_header_value_(true)
                , last_header_field_()
                , last_header_value_()
                , settings_()
                , error_()
                , parse_completed_(false)
                , url_()
                , result_()
            {
                http_parser_init(&parser_, parser_type);
                parser_.data = this;

                settings_.on_url = [](http_parser* parser, const char *at, size_t len) {
                    auto self = reinterpret_cast<http_parser_context*>(parser->data);
                    assert(self);

                    assert(at && len);

                    if(!self->url_.parse(at, len))
                    {
                        self->error_ = resval(error::http_parser_url_fail);
                        return 1;
                    }

                    return 0;
                };
                settings_.on_header_field = [](http_parser* parser, const char* at, size_t len) {
                    auto self = reinterpret_cast<http_parser_context*>(parser->data);
                    assert(self);

                    if(self->was_header_value_)
                    {
                        // new field started
                        if(!self->last_header_field_.empty())
                        {
                            // add new entry
                            self->result_.headers_[self->last_header_field_] = self->last_header_value_;
                            self->last_header_value_.clear();
                        }

                        self->last_header_field_ = std::string(at, len);
                        self->was_header_value_ = false;
                    }
                    else
                    {
                        // appending
                        self->last_header_field_ += std::string(at, len);
                    }

                    return 0;
                };
                settings_.on_header_value = [](http_parser* parser, const char* at, size_t len) {
                    auto self = reinterpret_cast<http_parser_context*>(parser->data);
                    assert(self);

                    if(!self->was_header_value_)
                    {
                        self->last_header_value_ = std::string(at, len);
                        self->was_header_value_ = true;
                    }
                    else
                    {
                        // appending
                        self->last_header_value_ += std::string(at, len);
                    }

                    return 0;
                };
                settings_.on_headers_complete = [](http_parser* parser) {
                    auto self = reinterpret_cast<http_parser_context*>(parser->data);
                    assert(self);

                    // add last entry if any
                    if(!self->last_header_field_.empty())
                    {
                        // add new entry
                        self->result_.headers_[self->last_header_field_] = self->last_header_value_;
                    }

                    return 0;
                };
                settings_.on_message_complete = [](http_parser* parser) {
                    auto self = reinterpret_cast<http_parser_context*>(parser->data);
                    assert(self);

                    // get host and port info from header entry "Host"
                    std::string host("");
                    int port = 0;
                    auto x = self->result_.headers_.find("host");
                    if(x != self->result_.headers_.end())
                    {
                        auto s = x->second;
                        auto colon = s.find_last_of(':');
                        if(colon == s.npos)
                        {
                            host = s;
                        }
                        else
                        {
                            host = s.substr(0, colon);
                            port = std::stoi(s.substr(colon+1));
                        }
                    }

                    // url info
                    self->result_.schema_ = self->url_.has_schema()?self->url_.schema():"HTTP";
                    self->result_.path_ = self->url_.has_path()?self->url_.path():"/";
                    self->result_.query_ = self->url_.has_query()?self->url_.query():"";
                    self->result_.fragment_ = self->url_.has_fragment()?self->url_.fragment():"";
                    self->result_.host_ = self->url_.has_host()?self->url_.host():host;

                    // determine port number
                    if(self->url_.has_port()) { self->result_.port_ = self->url_.port(); }
                    else if(port != 0) { self->result_.port_ = port; }
                    else
                    {
                        if(util::text::compare_no_case(self->result_.schema_, "HTTPS")) self->result_.port_ = 443;
                        else self->result_.port_ = 80;
                    }

                    // HTTP method
                    self->result_.method_ = http_method_str(static_cast<http_method>(parser->method));

                    // HTTP version
                    std::stringstream version;
                    version << parser->http_major << "." << parser->http_minor;
                    self->result_.http_version_ = version.str();

                    self->parse_completed_ = true;
                    return 0;
                };
            }

            ~http_parser_context()
            {}

        public:
            bool is_finished() const
            {
                // there was an error or parse completed.
                return !error_ || parse_completed_;
            }

            bool feed_data(const char* data, std::size_t offset, std::size_t length, http_parse_callback_type callback)
            {
                ::http_parser_execute(&parser_, &settings_, &data[offset], length);

                if(!error_)
                {
                    // there was an error
                    callback(nullptr, error_);
                    return true;
                }
                else if(parse_completed_)
                {
                    // finished
                    callback(&result_, resval());
                    return true;
                }
                else
                {
                    // need more data
                    return false;
                }
            }

        private:
            http_parser parser_;
            http_parser_settings settings_;
            bool was_header_value_;
            std::string last_header_field_;
            std::string last_header_value_;

            resval error_;
            bool parse_completed_;
            url_obj url_;
            http_parse_result result_;
        };

        resval parse_http_request(stream* input, http_parse_callback_type callback)
        {
            auto ctx = new http_parser_context(HTTP_REQUEST);
            assert(ctx);

            input->on_read([=](const char* data, std::size_t offset, std::size_t length, stream*, resval rv) {
                if(rv)
                {
                    if(ctx->feed_data(data, offset, length, callback))
                    {
                        // parse end
                        input->read_stop();
                        delete ctx;
                    }
                    else
                    {
                        // more reads required: keep reading!
                    }
                }
                else
                {
                    if(rv.code() == error::eof)
                    {
                        // EOF
                        if(!ctx->is_finished())
                        {
                            // HTTP request was not properly parsed.
                            callback(nullptr, resval(error::http_parser_incomplete));
                        }
                    }
                    else
                    {
                        // error
                        callback(nullptr, rv);
                    }
                    delete ctx;
                }
            });

            return input->read_start();
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

