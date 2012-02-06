#ifndef __HANDLE_H__
#define __HANDLE_H__

#include <cassert>
#include "base.h"
#include "callback.h"

namespace native
{
	namespace base
	{
		class handle;
		class stream;

		void _delete_handle(uv_handle_t* h);

		class handle
		{
		public:
			template<typename T>
			handle(T* x)
				: uv_handle_(reinterpret_cast<uv_handle_t*>(x))
			{
				//printf("handle(): %x\n", this);
				assert(uv_handle_);

				uv_handle_->data = new callbacks();
				assert(uv_handle_->data);
			}

			virtual ~handle()
			{
				//printf("~handle(): %x\n", this);
				uv_handle_ = nullptr;
			}

			handle(const handle& h)
				: uv_handle_(h.uv_handle_)
			{
				//printf("handle(const handle&): %x\n", this);
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
				callbacks::store(get()->data, cid_close, callback);
				uv_close(get(),
					[](uv_handle_t* h) {
						callbacks::invoke<F>(h->data, cid_close);
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
				callbacks::store(get()->data, cid_listen, callback);
				return uv_listen(get<uv_stream_t>(), backlog,
					[](uv_stream_t* s, int status) {
						callbacks::invoke<F>(s->data, cid_listen, status);
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
				callbacks::store(get()->data, cid_read_start, callback);

				return uv_read_start(get<uv_stream_t>(),
					[](uv_handle_t*, size_t suggested_size){
						return uv_buf_t { new char[suggested_size], suggested_size };
					},
					[](uv_stream_t* s, ssize_t nread, uv_buf_t buf){
						if(nread < 0)
						{
							assert(uv_last_error(s->loop).code == UV_EOF);
							callbacks::invoke<F>(s->data,
								cid_read_start,
								nullptr,
								static_cast<int>(nread));
						}
						else if(nread >= 0)
						{
							callbacks::invoke<F>(s->data,
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
