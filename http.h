#ifndef HTTP_H
#define HTTP_H

#include <cassert>
#include <map>
#include <memory>
#include <http_parser.h>
#include "base.h"

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

		class url_obj
		{
			friend class request;

		public:
			url_obj()
				: handle_(), buf_()
			{
				printf("url_obj() %x\n", this);
			}

			url_obj(const url_obj& c)
				: handle_(c.handle_), buf_(c.buf_)
			{
				printf("url_obj(const url_obj&) %x\n", this);
			}

			url_obj& operator =(const url_obj& c)
			{
				printf("url_obj::operator =(const url_obj&) %x\n", this);
				handle_ = c.handle_;
				buf_ = c.buf_;
				return *this;
			}

			~url_obj()
			{
				printf("~url_obj() %x\n", this);
			}

		public:
			std::string schema() const
			{
				if(has_schema()) return buf_.substr(handle_.field_data[UF_SCHEMA].off, handle_.field_data[UF_SCHEMA].len);
				return "HTTP";
			}

			std::string host() const
			{
				// TODO: if not specified, use REMOTE_ADDR
				if(has_schema()) return buf_.substr(handle_.field_data[UF_HOST].off, handle_.field_data[UF_HOST].len);
				return std::string();
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
			{}

		public:
			~response()
			{}

		public:
			void write(const std::string& s)
			{
			}

			void set_header(const std::string& key, const std::string& value)
			{
			}

		private:
			tcp* socket_;
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
				printf("request() %x callback_=%x\n", this, callback_);
				assert(server);

				// TODO: check error
				server->accept(socket_);
			}

		public:
			~request()
			{
				printf("~request() %x\n", this);
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
					printf("req->url: %x\n", &req->url_);

					return 0;
				};
				parser_settings_.on_headers_complete = [](http_parser* parser) {
					printf("on_headers_complete\n");

					auto req = reinterpret_cast<request*>(parser->data);

					//return 0;
					return 1; // do not parse body
				};
				parser_settings_.on_message_complete = [](http_parser* parser) {
					printf("on_message_complete\n");

					auto req = reinterpret_cast<request*>(parser->data);

					// parsing finished!
					std::shared_ptr<response> res(new response(req->socket_));

					printf("%x req->callback_=%x\n", req, req->callback_);

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
				printf("http() %x\n", this);
			}

			virtual ~http()
			{
				printf("~http() %x\n", this);

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

					printf("listen() this=%x\n", this);
					printf("listen() socket=%x\n", socket_.get());
					printf("listen() callback=%x\n", listen_callback_);

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
