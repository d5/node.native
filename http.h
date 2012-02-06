#ifndef HTTP_H
#define HTTP_H

#include <cassert>
#include <map>
#include <memory>
#include <sstream>
#include <http_parser.h>
#include "base.h"
#include "text.h"

using namespace native::base;

namespace native
{
	namespace http
	{
		class http;
		class request;
		class response;

		typedef void (*listen_callback_t)(request&, response&);

		class url_parse_exception { };
		class response_exception
		{
		public:
			response_exception(const std::string& message)
				: message_(message)
			{}

			const std::string& message() const { return message_; }

		private:
			std::string message_;
		};

		class url_obj
		{
			friend class request;

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

		class response
		{
			friend class request;

		private:
			response(tcp* client)
				: socket_(client)
				, headers_()
				, status_(200)
			{
				headers_["Content-Type"] = "text/html";
			}

		public:
			virtual ~response()
			{}

		public:
			template<typename F>
			bool end(const std::string& body, F callback)
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
				return socket_->write(str.c_str(), static_cast<int>(str.length()), callback);
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
			tcp* socket_;
			std::map<std::string, std::string, native::ci_less> headers_;
			int status_;
		};

		class request
		{
			friend class http;

		private:
			request(tcp* server, listen_callback_t callback)
				: socket_(new tcp)
				, callback_(callback)
				, parser_()
				, parser_settings_()
				, url_()
				, headers_()
			{
				//printf("request() %x callback_=%x\n", this, callback_);
				assert(server);

				// TODO: check error
				server->accept(socket_);
			}

		public:
			virtual ~request()
			{
				//printf("~request() %x\n", this);
			}

		public:
			bool parse()
			{
				http_parser_init(&parser_, HTTP_REQUEST);
				parser_.data = this;

				parser_settings_.on_url = [](http_parser* parser, const char *at, size_t len) {
					auto req = reinterpret_cast<request*>(parser->data);

					//  TODO: from_buf() can throw an exception: check
					req->url_.from_buf(at, len);

					return 0;
				};
				parser_settings_.on_headers_complete = [](http_parser* parser) {
					auto req = reinterpret_cast<request*>(parser->data);

					//return 0;
					return 1; // do not parse body
				};
				parser_settings_.on_message_complete = [](http_parser* parser) {
					auto req = reinterpret_cast<request*>(parser->data);

					// parsing finished!
					std::shared_ptr<response> res(new response(req->socket_));

					req->callback_(*req, *res);

					req->socket_->close([=](){
						delete req->socket_;
						delete req;
					});

					return 0;
				};

				socket_->read_start([=](const char* buf, int len){
					http_parser_execute(&parser_, &parser_settings_, buf, len);
				});

				return true;
			}

			const url_obj& url() const { return url_; }

		private:
			tcp* socket_;
			listen_callback_t callback_;

			http_parser parser_;

			// TODO: should be static
			http_parser_settings parser_settings_;

			url_obj url_;
			std::map<std::string, std::string> headers_;
		};

		class http
		{
		public:
			http(listen_callback_t listen_callback)
				: socket_(new tcp)
				, listen_callback_(listen_callback)
			{
				//printf("http() %x\n", this);
			}

			virtual ~http()
			{
				//printf("~http() %x\n", this);

				if(socket_)
				{
					socket_->close([](){});
				}
			}

		public:
			bool listen(const std::string& ip, int port)
			{
				if(!socket_->bind(ip, port)) return false;

				return socket_->listen([=](int status) {
					// TODO: error check - test if status is not 0

					auto req = new request(socket_.get(), listen_callback_);
					return req->parse();
				});
			}

		private:
			std::shared_ptr<tcp> socket_;
			listen_callback_t listen_callback_;
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
