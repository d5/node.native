#ifndef __DETAIL_STREAM_H__
#define __DETAIL_STREAM_H__

#include "base.h"
#include "handle.h"

namespace native
{
    namespace detail
    {
        class stream : public handle
        {
            typedef std::function<void(const char*, std::size_t, std::size_t, error)> on_read_callback_type;

        protected:
            stream(uv_stream_t* stream)
                : handle(reinterpret_cast<uv_handle_t*>(stream))
                , stream_(stream)
                , on_read_callback_()
            {
                assert(stream_);
            }

            virtual ~stream()
            {}

        public:
            void set_on_read_callback(on_read_callback_type callback)
            {
                on_read_callback_ = callback;
            }

            virtual void set_handle(uv_handle_t* h)
            {
                handle::set_handle(h);
                stream_ = reinterpret_cast<uv_stream_t*>(h);
            }

            std::size_t write_queue_size() const
            {
                if(stream_) return stream_->write_queue_size;
                else return 0;
            }

            error read_start()
            {
                bool res = uv_read_start(stream_, on_alloc, [](uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
                    auto self = reinterpret_cast<stream*>(handle->data);
                    assert(self);

                    if(nread < 0)
                    {
                        if(self->on_read_callback_) self->on_read_callback_(nullptr, 0, 0, get_last_error());
                    }
                    else
                    {
                        assert(nread <= buf.len);
                        if(nread > 0)
                        {
                            if(self->on_read_callback_) self->on_read_callback_(buf.base, 0, static_cast<std::size_t>(nread), error());
                        }
                    }

                    if(buf.base) delete buf.base;
                }) == 0;

                return res?error():get_last_error();
            }

            error read2_start()
            {
                assert(stream_->type == UV_NAMED_PIPE);
                assert(reinterpret_cast<uv_pipe_t*>(stream_)->ipc);

                bool res = uv_read2_start(stream_, on_alloc, [](uv_pipe_t* handle, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
                    auto self = reinterpret_cast<stream*>(handle->data);
                    assert(self);

                    // TODO: implement read2_start callback function.

                    if(buf.base) delete buf.base;
                }) == 0;

                return res?error():get_last_error();
            }

            error read_stop()
            {
                bool res = uv_read_stop(stream_) == 0;

                return res?error():get_last_error();
            }

            // TODO: Node.js implementation takes 4 parameter in callback function.
            error write(const char* data, int offset, int length, std::function<void(error)> callback)
            {
                auto req = new uv_write_t;
                assert(req);

                uv_buf_t buf;
                buf.base = const_cast<char*>(&data[offset]);
                buf.len = static_cast<size_t>(length);

                callbacks::store(lut(), cid_uv_write, callback);

                bool res = uv_write(req, stream_, &buf, 1, [](uv_write_t* req, int status){
                    auto self = reinterpret_cast<stream*>(req->handle->data);
                    assert(self);

                    callbacks::invoke<decltype(callback)>(self->lut(), cid_uv_write, status?get_last_error():error());

                    delete req;
                }) == 0;

                if(!res) delete req;

                return res?error():get_last_error();
            }

            error write2()
            {
                assert(stream_->type == UV_NAMED_PIPE);
                assert(reinterpret_cast<uv_pipe_t*>(stream_)->ipc);

                // TODO: implement stream::write2() function.
                return error();
            }

            // TODO: Node.js implementation takes 3 parameter in callback function.
            error shutdown(std::function<void(error)> callback=nullptr)
            {
                auto req = new uv_shutdown_t;
                assert(req);

                callbacks::store(lut(), cid_uv_shutdown, callback);

                bool res = uv_shutdown(req, stream_, [](uv_shutdown_t* req, int status){
                    auto self = reinterpret_cast<stream*>(req->handle->data);
                    assert(self);

                    callbacks::invoke<decltype(callback)>(self->lut(), cid_uv_shutdown, status?get_last_error():error());

                    delete req;
                }) == 0;

                if(!res) delete req;

                return res?error():get_last_error();
            }

            virtual error listen(int backlog, std::function<void(stream*, error)> callback) = 0;

            uv_stream_t* uv_stream() { return stream_; }
            const uv_stream_t* uv_stream() const { return stream_; }

        private:
            static uv_buf_t on_alloc(uv_handle_t* h, size_t suggested_size)
            {
                auto self = reinterpret_cast<stream*>(h->data);
                assert(self->stream_ == reinterpret_cast<uv_stream_t*>(h));

                // TODO: Better memory management needed here.
                // Something like 'slab' in the original node.js implementation.

                return uv_buf_t { new char[suggested_size], suggested_size };
            };

        private:
            uv_stream_t* stream_;
            std::function<void(const char*, int, int, error)> on_read_callback_;
        };
    }
}

#endif
