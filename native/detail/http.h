#ifndef __DETAIL_HTTP_H__
#define __DETAIL_HTTP_H__

#include "base.h"
#include "stream.h"

namespace native
{
    namespace detail
    {
        struct ci_less : std::binary_function<std::string, std::string, bool>
        {
            struct nocase_compare : public std::binary_function<unsigned char, unsigned char, bool>
            {
                bool operator()(const unsigned char& c1, const unsigned char& c2) const
                {
                    return tolower(c1) < tolower(c2);
                }
            };

            bool operator()(const std::string & s1, const std::string & s2) const
            {
                return std::lexicographical_compare(s1.begin(), s1.end(), // source range
                    s2.begin(), s2.end(), // dest range
                    nocase_compare()); // comparison
            }
        };

        class http_parse_result;
        bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

        class http_parse_result
        {
            friend bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

            typedef std::map<std::string, std::string, ci_less> headers_type;

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

        class http_parser_obj : public object
        {
            friend bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

        private:
            http_parser_obj()
                : object(1)
                , parser_()
                , was_header_value_(true)
                , last_header_field_()
                , last_header_value_()
                , settings_()
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
        bool parse_http_request(stream& input, std::function<void(const http_parse_result*, const char*)> callback)
        {
            http_parser_obj obj;
            http_parser_init(&obj.parser_, HTTP_REQUEST);
            obj.parser_.data = &obj;

            callbacks::store(obj.lut(), 0, callback);

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
                    callbacks::invoke<decltype(callback)>(obj->lut(), 0, nullptr, "Failed to parser URL in the request.");
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
                callbacks::invoke<decltype(callback)>(obj->lut(), 0, &obj->result_, nullptr);
                return 0;
            };

            return false;
        }
    }
}

#endif
