#ifndef __STREAM_H__
#define __STREAM_H__

#include "base.h"
#include "error.h"
#include "handle.h"
#include "callback.h"

namespace native
{
	namespace base
	{
		typedef uv_stream_info_t stream_info;

		class stream : public handle
		{
		public:
			template<typename T>
			stream(T* x)
				: handle(x)
			{ }

			bool listen(std::function<void(native::error)> callback, int backlog=128)
			{
				callbacks::store(get()->data, native::internal::uv_cid_listen, callback);
				return uv_listen(get<uv_stream_t>(), backlog, [](uv_stream_t* s, int status) {
                    callbacks::invoke<decltype(callback)>(s->data, native::internal::uv_cid_listen, status?uv_last_error(s->loop):error());
                }) == 0;
			}

			bool accept(stream* client)
			{
				return uv_accept(get<uv_stream_t>(), client->get<uv_stream_t>()) == 0;
			}

			bool read_start(std::function<void(const char* buf, ssize_t len)> callback)
			{
				return read_start<0>(callback);
			}

			template<int max_alloc_size>
			bool read_start(std::function<void(const char* buf, ssize_t len)> callback)
			{
				callbacks::store(get()->data, native::internal::uv_cid_read_start, callback);

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
							callbacks::invoke<decltype(callback)>(s->data, native::internal::uv_cid_read_start, nullptr, nread);
						}
						else if(nread >= 0)
						{
							callbacks::invoke<decltype(callback)>(s->data, native::internal::uv_cid_read_start, buf.base, nread);
						}
						delete buf.base;
					}) == 0;
			}

			bool read_stop()
			{
				return uv_read_stop(get<uv_stream_t>()) == 0;
			}

			// TODO: implement read2_start()

			bool write(const char* buf, int len, std::function<void(error)> callback)
			{
				uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(buf), len } };
				callbacks::store(get()->data, native::internal::uv_cid_write, callback);
				return uv_write(new uv_write_t, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
					callbacks::invoke<decltype(callback)>(req->handle->data, native::internal::uv_cid_write, status?uv_last_error(req->handle->loop):error());
					delete req;
				}) == 0;
			}

			bool write(const std::string& buf, std::function<void(error)> callback)
            {
                uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(buf.c_str()), buf.length()} };
                callbacks::store(get()->data, native::internal::uv_cid_write, callback);
                return uv_write(new uv_write_t, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
                    callbacks::invoke<decltype(callback)>(req->handle->data, native::internal::uv_cid_write, status?uv_last_error(req->handle->loop):error());
                    delete req;
                }) == 0;
            }

            bool write(const std::vector<char>& buf, std::function<void(error)> callback)
            {
                uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(&buf[0]), buf.size() } };
                callbacks::store(get()->data, native::internal::uv_cid_write, callback);
                return uv_write(new uv_write_t, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
                    callbacks::invoke<decltype(callback)>(req->handle->data, native::internal::uv_cid_write, status?uv_last_error(req->handle->loop):error());
                    delete req;
                }) == 0;
            }

            // TODO: implement write2()

			bool shutdown(std::function<void(error)> callback)
			{
				callbacks::store(get()->data, native::internal::uv_cid_shutdown, callback);
				return uv_shutdown(new uv_shutdown_t, get<uv_stream_t>(), [](uv_shutdown_t* req, int status) {
					callbacks::invoke<decltype(callback)>(req->handle->data, native::internal::uv_cid_shutdown, status?uv_last_error(req->handle->loop):error());
					delete req;
				}) == 0;
			}

			bool export_to(stream_info* info)
			{
				return uv_export(get<uv_stream_t>(), info) == 0;
			}

			bool import_from(stream_info* info)
			{
				return uv_import(get<uv_stream_t>(), info) == 0;
			}
		};
	}
}

#endif
