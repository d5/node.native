#ifndef HTTP_H
#define HTTP_H

#include <cassert>
#include <memory>
#include <http_parser.h>
#include "base.h"

using namespace native::base;

namespace native
{
	namespace http
	{
		class url_parse_exception { };

		class url
		{
		public:
			url()
				: handle_(), buf_()
			{ }

			url(const url&) = default;
			url& operator =(const url&) = default;
			~url() = default;

		public:
			void from_buf(const char* data, std::size_t len, bool is_connect=false)
			{
				assert(data && len);

				if(http_parser_parse_url(data, len, is_connect, &handle_) != 0)
				{
					// failed for some reason
					// TODO: let the caller know the error code (or error message)
					throw url_parse_exception();
				}
			}

			bool has_schema() const
			{
				return handle_.field_set & (1<<UF_SCHEMA);
			}

			std::string schema() const
			{
				if(has_schema())
				{
					return buf_.substr(handle_.field_data[UF_SCHEMA].off, handle_.field_data[UF_SCHEMA].len);
				}

				return std::string();
			}

			bool has_host() const
			{
				return handle_.field_set & (1<<UF_HOST);
			}

			std::string host() const
			{
				if(has_host())
				{
					return buf_.substr(handle_.field_data[UF_HOST].off, handle_.field_data[UF_HOST].len);
				}

				return std::string();
			}

			bool has_port() const
			{
				return handle_.field_set & (1<<UF_PORT);
			}

			int port() const
			{
				return static_cast<int>(handle_.port);
			}

			bool has_path() const
			{
				return handle_.field_set & (1<<UF_PATH);
			}

			std::string path() const
			{
				if(has_path())
				{
					return buf_.substr(handle_.field_data[UF_PATH].off, handle_.field_data[UF_PATH].len);
				}

				return std::string();
			}

			bool has_query() const
			{
				return handle_.field_set & (1<<UF_QUERY);
			}

			std::string query() const
			{
				if(has_query())
				{
					return buf_.substr(handle_.field_data[UF_QUERY].off, handle_.field_data[UF_QUERY].len);
				}

				return std::string();
			}

			bool has_fragment() const
			{
				return handle_.field_set & (1<<UF_FRAGMENT);
			}

			std::string fragment() const
			{
				if(has_fragment())
				{
					return buf_.substr(handle_.field_data[UF_FRAGMENT].off, handle_.field_data[UF_FRAGMENT].len);
				}

				return std::string();
			}

		private:
			http_parser_url handle_;
			std::string buf_;
		};

		class request
		{
		public:
			request(tcp* server)
				: client_(new tcp)
				, parser_()
				, parser_settings_()
				, url_()
			{
				assert(server);

				// TODO: check error
				server->accept(client_);
			}

			virtual ~request()
			{
			}

			template<typename callback_t>
			bool parse(callback_t callback)
			{
				http_parser_init(&parser_, HTTP_REQUEST);
				parser_.data = this;

				http_parser_settings settings;
				settings.on_url = [](http_parser* parser, const char *at, size_t len) {
					auto req = reinterpret_cast<request*>(parser->data);
					req->url_.from_buf(at, len);
					return 0;
				};
				settings.on_headers_complete = [](http_parser* parser) {
					printf("on_headers_complete\n");

					auto req = reinterpret_cast<request*>(parser->data);

					// invoke callback
					//callback(req);

					//return 0;
					return 1; // do not parse body
				};
				settings.on_message_complete = [](http_parser* parser) {
					printf("on_message_complete\n");
					return 0;
				};

				client_->read_start([=](const char* buf, int len){
					http_parser_execute(&parser_, &parser_settings_, buf, len);
				});

				return false;
			}

		private:
			std::shared_ptr<tcp> client_;

			http_parser parser_;
			// TODO: should be static
			http_parser_settings parser_settings_;
			url url_;
		};

		class http
		{
		public:
			http()
				: server_(new tcp)
			{
			}

			virtual ~http()
			{
				if(server_)
				{
					server_->close([](){});
				}
			}

		public:
			template<typename callback_t>
			bool listen(const std::string& ip, int port, callback_t callback)
			{
				if(server_->bind(ip, port))
				{
					return server_->listen([=] {
						std::shared_ptr<request> req(new request(server_.get()));
						return req->parse(callback);
					});
				}

				// callback function will not be called!
				return false;
			}

		private:
			std::shared_ptr<tcp> server_;
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
