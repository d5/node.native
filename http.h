#ifndef HTTP_H
#define HTTP_H

namespace native
{
	namespace http
	{
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

		class url_parse_exception { };

		class url
		{
		public:
			url(const char* data, std::size_t len, bool is_connect=false)
				: handle_(), buf_(data, len)
			{
				assert(data && len);

				if(http_parser_parse_url(data, len, is_connect, &handle_) != 0)
				{
					// failed for some reason
					// TODO: let the caller know the error code (or error message)
					throw url_parse_exception();
				}
			}

			url() = default;
			url(const url&) = default;
			url& operator =(const url&) = default;
			~url() = default;

		public:
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

		class parser
		{
			typedef http_parser_type parser_type;

		public:
			parser(parser_type type)
				: handle_(), schema_(), host_(), port_(), path_(), query_(), fragment_(), headers_(), body_()
			{
				http_parser_init(&handle_, type);
			}

			virtual ~parser() {
			}

		protected:
			template<typename cb_t>
			void set_message_begin_callback(cb_t callback)
			{
			}

			template<typename cb_t>
			void set_url_callback(cb_t callback)
			{
			}

			std::size_t parse(const char* data, std::size_t len)
			{
				handle_.data = this;

				http_parser_settings settings;
				settings.on_url = [](http_parser* h, const char *at, size_t length) {
					auto p = reinterpret_cast<parser*>(h->data);
					assert(p);

					url u(at, length);

					p->schema_ = u.has_schema() ? u.schema() : "HTTP";
					p->host_ = u.has_host() ? u.host() : "";
					p->port_ = u.has_port() ? u.port() : (p->schema_ == "HTTP" ? 80 : 443);
					p->path_ = u.has_path() ? u.path() : "/";
					p->query_ = u.has_query() ? u.query() : "";
					p->fragment_ = u.has_fragment() ? u.fragment() : "";

					return 0;
				};

				return http_parser_execute(&handle_, &settings, data, len);
			}

		private:
			// copy prevention
			parser(const parser&);
			void operator =(const parser&);

		private:
			http_parser handle_;
			std::string schema_;
			std::string host_;
			int port_;
			std::string path_;
			std::string query_;
			std::string fragment_;
			std::map<std::string, std::string> headers_;
			std::string body_;
		};

		class request : public parser
		{
		public:
			request()
				: parser(HTTP_REQUEST)
			{
			}

			virtual ~request()
			{
			}
		};

		class response : public parser
		{
		public:
			response()
				: parser(HTTP_RESPONSE)
			{
			}

			virtual ~response()
			{
			}
		};
	}
}

#endif
