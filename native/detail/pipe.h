#ifndef __DETAIL_PIPE_H__
#define __DETAIL_PIPE_H__

#include "base.h"
#include "stream.h"

namespace native
{
    namespace detail
    {
        class pipe : public stream
        {
        public:
            pipe(bool ipc=false)
                : stream(reinterpret_cast<uv_stream_t*>(&pipe_))
            {
                int r = uv_pipe_init(uv_default_loop(), &pipe_, ipc?1:0);
                assert(r == 0);

                pipe_.data = this;
            }

        private:
            virtual ~pipe()
            {}

        public:
            virtual error bind(const std::string& name)
            {
                bool res = uv_pipe_bind(&pipe_, name.c_str());

                return res?error():get_last_error();
            }

            virtual error listen(int backlog, std::function<void(stream*, error)> callback)
            {
                callbacks::store(lut(), cid_uv_listen, callback);
                bool res = uv_listen(reinterpret_cast<uv_stream_t*>(&pipe_), backlog, [](uv_stream_t* handle, int status) {
                    auto self = reinterpret_cast<pipe*>(handle->data);
                    assert(self);

                    assert(status == 0);

                    // TODO: what about 'ipc' parameter in ctor?
                    auto client_obj = new pipe;
                    assert(client_obj);

                    int r = uv_accept(handle, reinterpret_cast<uv_stream_t*>(&client_obj->pipe_));
                    assert(r == 0);

                    callbacks::invoke<decltype(callback)>(self->lut(), cid_uv_listen, client_obj, error());
                }) == 0;

                return res?error():get_last_error();
            }

            virtual void open(int fd)
            {
                uv_pipe_open(&pipe_, fd);
            }

            // TODO: Node.js implementation takes 5 parameter in callback function.
            virtual void connect(const std::string& name, std::function<void(error, bool, bool)> callback)
            {
                auto req = new uv_connect_t;
                assert(req);

                callbacks::store(lut(), cid_uv_connect, callback);

                uv_pipe_connect(req, &pipe_, name.c_str(), [](uv_connect_t* req, int status) {
                    auto self = reinterpret_cast<pipe*>(req->handle->data);
                    assert(self);

                    bool readable = false;
                    bool writable = false;

                    if(status==0)
                    {
                        readable = uv_is_readable(req->handle) != 0;
                        writable = uv_is_writable(req->handle) != 0;
                    }

                    callbacks::invoke<decltype(callback)>(self->lut(), cid_uv_connect, status?get_last_error():error(), readable, writable);

                    delete req;
                });
            }
        private:
            uv_pipe_t pipe_;
        };
    }
}

#endif
