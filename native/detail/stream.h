#ifndef __DETAIL_STREAM_H__
#define __DETAIL_STREAM_H__

#include "base.h"
#include "handle.h"

namespace native
{
    namespace detail
    {
        class tcp;

        class stream : public handle
        {
            typedef std::function<void(const char*, std::size_t, std::size_t, stream*, error)> on_read_callback_type;
            typedef std::function<void(error)> on_complete_callback_type;

        protected:
            stream(uv_stream_t* stream)
                : handle(reinterpret_cast<uv_handle_t*>(stream))
                , stream_(stream)
                , on_read_()
                , on_complete_()
            {
                assert(stream_);
            }

            virtual ~stream()
            {}

        public:
            void on_read(on_read_callback_type callback)
            {
                on_read_ = callback;
            }

            void on_complete(on_complete_callback_type callback)
            {
                on_complete_ = callback;
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
                bool res = false;
                bool ipc_pipe = stream_->type == UV_NAMED_PIPE && reinterpret_cast<uv_pipe_t*>(stream_)->ipc;

                if(ipc_pipe)
                {
                    bool res = uv_read2_start(stream_, on_alloc, [](uv_pipe_t* handle, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
                       auto self = reinterpret_cast<stream*>(handle->data);
                       assert(self);
                       self->after_read_(reinterpret_cast<uv_stream_t*>(handle), nread, buf, pending);
                   }) == 0;
                }
                else
                {
                    res = uv_read_start(stream_, on_alloc, [](uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
                        auto self = reinterpret_cast<stream*>(handle->data);
                        assert(self);
                        self->after_read_(handle, nread, buf, UV_UNKNOWN_HANDLE);
                    }) == 0;
                }

                return res?error():get_last_error();
            }

            error read_stop()
            {
                bool res = uv_read_stop(stream_) == 0;
                return res?error():get_last_error();
            }

            error write(const char* data, int offset, int length, stream* send_stream=nullptr)
            {
                bool res = false;
                bool ipc_pipe = stream_->type == UV_NAMED_PIPE && reinterpret_cast<uv_pipe_t*>(stream_)->ipc;

                auto req = new uv_write_t;
                assert(req);

                uv_buf_t buf;
                buf.base = const_cast<char*>(&data[offset]);
                buf.len = static_cast<size_t>(length);

                if(ipc_pipe)
                {
                    res = uv_write2(req, stream_, &buf, 1, send_stream?send_stream->uv_stream():nullptr, [](uv_write_t* req, int status) {
                        auto self = reinterpret_cast<stream*>(req->handle->data);
                        assert(self);
                        if(self->on_complete_) self->on_complete_(status?get_last_error():error());
                        if(req) delete req;
                    }) == 0;
                }
                else
                {
                    res = uv_write(req, stream_, &buf, 1, [](uv_write_t* req, int status) {
                        auto self = reinterpret_cast<stream*>(req->handle->data);
                        assert(self);
                        if(self->on_complete_) self->on_complete_(status?get_last_error():error());
                        if(req) delete req;
                    }) == 0;
                }

                if(!res) delete req;
                return res?error():get_last_error();
            }

            error shutdown()
            {
                auto req = new uv_shutdown_t;
                assert(req);

                bool res = uv_shutdown(req, stream_, [](uv_shutdown_t* req, int status){
                    auto self = reinterpret_cast<stream*>(req->handle->data);
                    assert(self);
                    if(self->on_complete_) self->on_complete_(status?get_last_error():error());
                    delete req;
                }) == 0;

                if(!res) delete req;
                return res?error():get_last_error();
            }

            virtual error listen(int backlog, std::function<void(stream*, error)> callback) = 0;

            uv_stream_t* uv_stream() { return stream_; }
            const uv_stream_t* uv_stream() const { return stream_; }

        protected:
            virtual stream* accept_new_() { return nullptr; }

        private:
            static uv_buf_t on_alloc(uv_handle_t* h, size_t suggested_size)
            {
                auto self = reinterpret_cast<stream*>(h->data);
                assert(self->stream_ == reinterpret_cast<uv_stream_t*>(h));

                // TODO: Better memory management needed here.
                // Something like 'slab' in the original node.js implementation.

                return uv_buf_t { new char[suggested_size], suggested_size };
            };

            void after_read_(uv_stream_t* handle, ssize_t nread, uv_buf_t buf, uv_handle_type pending)
            {
                if(nread < 0)
                {
                    // error or EOF: invoke "onread" callback
                    if(on_read_) on_read_(nullptr, 0, 0, nullptr, get_last_error());
                }
                else
                {
                    assert(nread <= buf.len);
                    if(nread > 0)
                    {
                        // see uv_read2_start()
                        if(pending == UV_TCP)
                        {
                            auto accepted = accept_new_();
                            assert(accepted);

                            // invoke "onread" callback
                            if(on_read_) on_read_(buf.base, 0, nread, accepted, error());
                        }
                        else
                        {
                            // Only TCP supported
                            assert(pending == UV_UNKNOWN_HANDLE);

                            // invoke "onread" callback
                            if(on_read_) on_read_(buf.base, 0, nread, nullptr, error());
                        }
                    }
                }

                if(buf.base) delete buf.base;
            }

        private:
            uv_stream_t* stream_;

            on_read_callback_type on_read_;
            on_complete_callback_type on_complete_;
        };
    }
}

#endif
