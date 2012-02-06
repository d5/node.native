#ifndef __STREAM_H__
#define __STREAM_H__

#include <cassert>
#include "base.h"
#include "handle.h"
#include "callback.h"

namespace native
{
	namespace base
	{
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

			template<typename F>
			bool read_start(F callback)
			{
				return read_start<0, F>(callback);
			}

			template<int max_alloc_size, typename F>
			bool read_start(F callback)
			{
				callbacks::store(get()->data, cid_read_start, callback);

				return uv_read_start(get<uv_stream_t>(),
					[](uv_handle_t*, size_t suggested_size){
						if(!max_alloc_size)
						{
							return uv_buf_t { new char[suggested_size], suggested_size };
						}
						else
						{
							auto size = max_alloc_size > suggested_size ? suggested_size : max_alloc_size;
							return uv_buf_t { new char[size], size };
						}
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
	}
}

#endif
