#ifndef __HTTP_H__
#define __HTTP_H__

#include <sstream>
#include <http_parser.h>
#include "base.h"
#include "handle.h"
#include "net.h"
#include "text.h"
#include "callback.h"

namespace native
{
	namespace http
	{
		class url_parse_exception : public native::exception
		{
		public:
			url_parse_exception(const std::string& message="Failed to parse URL.")
				: native::exception(message)
			{}
		};

		class response_exception : public native::exception
		{
		public:
			response_exception(const std::string& message="HTTP respsonse error.")
				: native::exception(message)
			{}
		};

		class url_obj
		{
			friend class client_context;

		public:
			url_obj()
				: handle_(), buf_()
			{
				//printf("url_obj() %x\n", this);
			}

			url_obj(const url_obj& c)
				: handle_(c.handle_), buf_(c.buf_)
			{
				//printf("url_obj(const url_obj&) %x\n", this);
			}

			url_obj& operator =(const url_obj& c)
			{
				//printf("url_obj::operator =(const url_obj&) %x\n", this);
				handle_ = c.handle_;
				buf_ = c.buf_;
				return *this;
			}

			~url_obj()
			{
				//printf("~url_obj() %x\n", this);
			}

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
			void from_buf(const char* buf, std::size_t len, bool is_connect=false)
			{
				// TODO: validate input parameters

				buf_ = std::string(buf, len);
				if(http_parser_parse_url(buf, len, is_connect, &handle_) != 0)
				{
					// failed for some reason
					// TODO: let the caller know the error code (or error message)
					throw url_parse_exception();
				}
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

		class client_context;
		typedef std::shared_ptr<client_context> http_client_ptr;

		class response
		{
			friend class client_context;

		private:
			response(client_context* client, native::net::tcp* socket)
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
				return socket_->write(str.c_str(), static_cast<int>(str.length()), [=](error e) {
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
				default: throw response_exception("Not supported status code.");
				}
			}

		private:
			http_client_ptr client_;
			native::net::tcp* socket_;
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

			const std::string& get_header(const std::string& key) const
			{
				auto it = headers_.find(key);
				if(it != headers_.end()) return it->second;
				return default_value_;
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

                        std::string get_body (void)
                        {
                                return body_;
                        }

		private:
			url_obj url_;
			std::map<std::string, std::string, native::text::ci_less> headers_;
                        std::string body_;
                        std::string default_value_;
		};

		class client_context
		{
			friend class http;

		private:
			client_context(native::net::tcp* server)
				: socket_(nullptr)
				, parser_()
				, was_header_value_(true)
				, last_header_field_()
				, last_header_value_()
				, parser_settings_()
				, request_(nullptr)
				, response_(nullptr)
				, callback_lut_(new callbacks(1))
			{
				//printf("request() %x callback_=%x\n", this, callback_);
				assert(server);

				// TODO: check error
				socket_ = std::shared_ptr<native::net::tcp> (new native::net::tcp);
				server->accept(socket_.get());
			}

		public:
			~client_context()
			{
				if(request_)
				{
					delete request_;
					request_ = nullptr;
				}

				if(response_)
				{
					delete response_;
					response_ = nullptr;
				}

				if(callback_lut_)
				{
					delete callback_lut_;
					callback_lut_ = nullptr;
				}

				if(socket_.use_count())
				{
					socket_->close([=](){});
				}
			}

		private:
			bool parse(std::function<void(request&, response&)> callback)
			{
				request_ = new request;
				response_ = new response(this, socket_.get());

				http_parser_init(&parser_, HTTP_REQUEST);
				parser_.data = this;

				// store callback object
				callbacks::store(callback_lut_, 0, callback);

				parser_settings_.on_url = [](http_parser* parser, const char *at, size_t len) {
					auto client = reinterpret_cast<client_context*>(parser->data);

					//  TODO: from_buf() can throw an exception: check
					client->request_->url_.from_buf(at, len);

					return 0;
				};
				parser_settings_.on_header_field = [](http_parser* parser, const char* at, size_t len) {
					auto client = reinterpret_cast<client_context*>(parser->data);

					if(client->was_header_value_)
					{
						// new field started
						if(!client->last_header_field_.empty())
						{
							// add new entry
							client->request_->headers_[client->last_header_field_] = client->last_header_value_;
							client->last_header_value_.clear();
						}

						client->last_header_field_ = std::string(at, len);
						client->was_header_value_ = false;
					}
					else
					{
						// appending
						client->last_header_field_ += std::string(at, len);
					}
					return 0;
				};
				parser_settings_.on_header_value = [](http_parser* parser, const char* at, size_t len) {
					auto client = reinterpret_cast<client_context*>(parser->data);

					if(!client->was_header_value_)
					{
						client->last_header_value_ = std::string(at, len);
						client->was_header_value_ = true;
					}
					else
					{
						// appending
						client->last_header_value_ += std::string(at, len);
					}
					return 0;
				};
				parser_settings_.on_headers_complete = [](http_parser* parser) {
					auto client = reinterpret_cast<client_context*>(parser->data);

					// add last entry if any
					if(!client->last_header_field_.empty()) {
						// add new entry
						client->request_->headers_[client->last_header_field_] = client->last_header_value_;
					}

					return 0; // 1 to prevent reading of message body.
				};
				parser_settings_.on_body = [](http_parser* parser, const char* at, size_t len) {
                                        //printf("on_body, len of 'char* at' is %d\n", len);
					auto client = reinterpret_cast<client_context*>(parser->data);
                                        client->request_->body_ = std::string(at, len);
					return 0;
				};
				parser_settings_.on_message_complete = [](http_parser* parser) {
                                        //printf("on_message_complete, so invoke the callback.\n");
					auto client = reinterpret_cast<client_context*>(parser->data);
					// invoke stored callback object
					callbacks::invoke<decltype(callback)>(client->callback_lut_, 0, *client->request_, *client->response_);
					return 1; // 0 or 1?
				};

				socket_->read_start([=](const char* buf, int len){
					if (buf == 0x00 && len == -1) {
						response_->set_status(500);
					} else {
						http_parser_execute(&parser_, &parser_settings_, buf, len);
					}
				});

				return true;
			}

		private:
			http_parser parser_;
			http_parser_settings parser_settings_;
			bool was_header_value_;
			std::string last_header_field_;
			std::string last_header_value_;

			std::shared_ptr<native::net::tcp> socket_;
			request* request_;
			response* response_;

			callbacks* callback_lut_;
		};

		class http
		{
		public:
			http()
				: socket_(new native::net::tcp)
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

				if(!socket_->listen([=](error e) {
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
			std::shared_ptr<native::net::tcp> socket_;
		};

		typedef http_method method;
		typedef http_parser_url_fields url_fields;
		typedef http_errno error;

		const char* get_error_name(error err)
		{
			return http_errno_name(err);
		}

		const char* get_error_description(error err)
		{
			return http_errno_description(err);
		}

		const char* get_method_name(method m)
		{
			return http_method_str(m);
		}
	}
}

#endif
