#ifndef BASE_H
#define BASE_H

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <utility>
#include <uv.h>
#include <http_parser.h>
#include "callback.h"

namespace native
{
	namespace base
	{
		class loop;
		class handle;
		class err;
		class handle;
		class stream;
		class tcp;
		class udp;
		class pipe;
		class req;
		class shutdown;
		class write;
		class connect;
		class timer;
		class async;
		class buf;

		typedef std::shared_ptr<handle> handle_ptr;
		typedef std::shared_ptr<stream> stream_ptr;
		typedef std::shared_ptr<tcp> tcp_ptr;

		void _delete_handle(uv_handle_t* h);

		typedef uv_req_type req_type;
		typedef uv_membership membership;

		class exception
		{
		public:
			exception(const std::string& message)
				: message_(message)
			{}

			const std::string& message() const { return message_; }

		private:
			std::string message_;
		};

		typedef sockaddr_in ip4_addr;
		typedef sockaddr_in6 ip6_addr;

		ip4_addr to_ip4_addr(const std::string& ip, int port) { return uv_ip4_addr(ip.c_str(), port); }
		ip6_addr to_ip6_addr(const std::string& ip, int port) { return uv_ip6_addr(ip.c_str(), port); }

		bool from_ip4_addr(ip4_addr* src, std::string& ip, int& port)
		{
			char dest[16];
			if(uv_ip4_name(src, dest, 16) == 0)
			{
				ip = dest;
				port = static_cast<int>(ntohs(src->sin_port));
				return true;
			}
			return false;
		}

		bool from_ip6_addr(ip6_addr* src, std::string& ip, int& port)
		{
			char dest[46];
			if(uv_ip6_name(src, dest, 46) == 0)
			{
				ip = dest;
				port = static_cast<int>(ntohs(src->sin6_port));
				return true;
			}
			return false;
		}

		class err
		{
		public:
			err(uv_err_t e) : uv_err_(e) {}
			~err() = default;

		public:
			const char* name() const { return uv_err_name(uv_err_); }
			const char* str() const { return uv_strerror(uv_err_); }

		private:
			uv_err_t uv_err_;
		};

		// uv_loop_t wrapper
		class loop
		{
		public:
			loop(bool use_default=false)
				: uv_loop_(use_default ? uv_default_loop() : uv_loop_new())
			{ }

			~loop()
			{
				if(uv_loop_)
				{
					uv_loop_delete(uv_loop_);
					uv_loop_ = nullptr;
				}
			}

			uv_loop_t* get() { return uv_loop_; }

			int run() { return uv_run(uv_loop_); }
			int run_once() { return uv_run_once(uv_loop_); }
			static int run_default() { return uv_run(uv_default_loop()); }
			static int run_default_once() { return uv_run_once(uv_default_loop()); }

			void ref() { uv_ref(uv_loop_); }
			void unref() { uv_unref(uv_loop_); }
			void update_time() { uv_update_time(uv_loop_); }
			int64_t now() { return uv_now(uv_loop_); }

			err last_error() { return uv_last_error(uv_loop_); }
			static err last_error_default() { return uv_last_error(uv_default_loop()); }

		private:
			loop(const loop&);
			void operator =(const loop&);

		private:
			uv_loop_t* uv_loop_;
		};

		class req
		{
		protected:
			req() {};
			virtual ~req() {}

		public:
			virtual uv_req_t* get() = 0;

			req_type type() { return get()->type; }
			void* data() { return get()->data; }
		};

		class handle
		{
		public:
			template<typename T>
			handle(T* x)
				: uv_handle_(reinterpret_cast<uv_handle_t*>(x))
			{
				printf("handle(): %x\n", this);
				assert(uv_handle_);

				uv_handle_->data = new callbacks();
				assert(uv_handle_->data);
			}

			virtual ~handle()
			{
				printf("~handle(): %x\n", this);
				uv_handle_ = nullptr;
			}

			handle(const handle& h)
				: uv_handle_(h.uv_handle_)
			{
				printf("handle(const handle&): %x\n", this);
			}

		public:
			template<typename T=uv_handle_t>
			T* get() { return reinterpret_cast<T*>(uv_handle_); }

			template<typename T=uv_handle_t>
			const T* get() const { return reinterpret_cast<T*>(uv_handle_); }

			bool is_active() { return uv_is_active(get()) != 0; }

			template<typename F>
			void close(F callback)
			{
				callbacks::store(get(), cid_close, callback);
				uv_close(get(),
					[](uv_handle_t* h) {
						callbacks::invoke<F>(h, cid_close);
						_delete_handle(h);
					});
			}

			handle& operator =(const handle& h)
			{
				uv_handle_ = h.uv_handle_;
				return *this;
			}

		protected:
			uv_handle_t* uv_handle_;
		};

		class stream : public handle
		{
		public:
			template<typename T>
			stream(T* x)
				: handle(x)
			{ }

			/*
			int shutdown(shutdown_cb cb)
			{
				auto req = new uv_shutdown_t;
				req->data = this;
				return uv_shutdown(req, get<uv_stream_t>(), cb);
			}
			*/

			template<typename F>
			bool listen(F callback, int backlog=128)
			{
				callbacks::store(get(), cid_listen, callback);
				return uv_listen(get<uv_stream_t>(), backlog,
					[](uv_stream_t* s, int status) {
						callbacks::invoke<F>(s, cid_listen, status);
					}) == 0;
			}

			bool accept(stream* client)
			{
				return uv_accept(get<uv_stream_t>(), client->get<uv_stream_t>()) == 0;
			}

			// TODO: read callback that pass buffer as std::string<> or std::vector<>?
			template<typename F>
			bool read_start(F callback)
			{
				callbacks::store(get(), cid_read_start, callback);

				return uv_read_start(get<uv_stream_t>(),
					[](uv_handle_t*, size_t suggested_size){
						return uv_buf_t { new char[suggested_size], suggested_size };
					},
					[](uv_stream_t* s, ssize_t nread, uv_buf_t buf){
						if(nread < 0)
						{
							assert(uv_last_error(s->loop).code == UV_EOF);
							callbacks::invoke<F>(s,
								cid_read_start,
								nullptr,
								static_cast<int>(nread));
						}
						else if(nread >= 0)
						{
							callbacks::invoke<F>(s,
								cid_read_start,
								buf.base,
								static_cast<int>(nread));
						}
						delete buf.base;
					}) == 0;
			}

			bool read_stop()
			{
				return uv_read_stop(get<uv_stream_t>()) == 0;
			}

			// TODO: implement read2_start()
			//int read2_start(alloc_cb a, read2_cb r) { return uv_read2_start(get<uv_stream_t>(), a, r); }

			// TODO: add overloading that accepts std::vector<> or std::string<>
			template<typename F>
			bool write(const char* buf, int len, F callback)
			{
				// TODO: const_cast<>!!!
				uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(buf), len } };

				callbacks::store(get(), cid_write, callback);

				uv_write_t* w = new uv_write_t;
				return uv_write(w, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
					callbacks::invoke<F>(req->handle, cid_write, status);
				}) == 0;
			}

			// TODO: implement write2()
			//int write2(buf& b, stream& send_handle, write_cb cb);
		};

		class tcp : public stream
		{
		public:
			template<typename X>
			tcp(X* x)
				: stream(x)
			{ }

		public:
			tcp()
				: stream(new uv_tcp_t)
			{
				uv_tcp_init(uv_default_loop(), get<uv_tcp_t>());
			}

			tcp(loop& l)
				: stream(new uv_tcp_t)
			{
				uv_tcp_init(l.get(), get<uv_tcp_t>());
			}

			bool nodelay(bool enable) { return uv_tcp_nodelay(get<uv_tcp_t>(), enable?1:0) == 0; }
			bool keepalive(bool enable, unsigned int delay) { return uv_tcp_keepalive(get<uv_tcp_t>(), enable?1:0, delay) == 0; }
			bool simultanious_accepts(bool enable) { return uv_tcp_simultaneous_accepts(get<uv_tcp_t>(), enable?1:0) == 0; }

			bool bind(const std::string& ip, int port) { return uv_tcp_bind(get<uv_tcp_t>(), uv_ip4_addr(ip.c_str(), port)) == 0; }
			bool bind6(const std::string& ip, int port) { return uv_tcp_bind6(get<uv_tcp_t>(), uv_ip6_addr(ip.c_str(), port)) == 0; }

			bool getsockname(bool& ip4, std::string& ip, int& port)
			{
				struct sockaddr_storage addr;
				int len = sizeof(addr);
				if(uv_tcp_getsockname(get<uv_tcp_t>(), reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
				{
					ip4 = (addr.ss_family == AF_INET);
					if(ip4) return from_ip4_addr(reinterpret_cast<ip4_addr*>(&addr), ip, port);
					else return from_ip6_addr(reinterpret_cast<ip6_addr*>(&addr), ip, port);
				}
				return false;
			}

			bool getpeername(bool& ip4, std::string& ip, int& port)
			{
				struct sockaddr_storage addr;
				int len = sizeof(addr);
				if(uv_tcp_getpeername(get<uv_tcp_t>(), reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
				{
					ip4 = (addr.ss_family == AF_INET);
					if(ip4) return from_ip4_addr(reinterpret_cast<ip4_addr*>(&addr), ip, port);
					else return from_ip6_addr(reinterpret_cast<ip6_addr*>(&addr), ip, port);
				}
				return false;
			}
		};

		void _delete_handle(uv_handle_t* h)
		{
			assert(h);

			// clean up SCM
			if(h->data)
			{
				delete reinterpret_cast<callbacks*>(h->data);
				h->data = nullptr;
			}

			switch(h->type)
			{
				case UV_TCP: delete reinterpret_cast<uv_tcp_t*>(h); break;
				default: assert(0); break;
			}
		}
	}
}

#endif
