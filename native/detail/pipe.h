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
            virtual resval bind(const std::string& name)
            {
                return run_(uv_pipe_bind, &pipe_, name.c_str());
            }

            virtual void open(int fd)
            {
                uv_pipe_open(&pipe_, fd);
            }

            virtual void connect(const std::string& name)
            {
                auto req = new uv_connect_t;
                assert(req);

                uv_pipe_connect(req, &pipe_, name.c_str(), [](uv_connect_t* req, int status){
                    auto self = reinterpret_cast<pipe*>(req->handle->data);
                    assert(self);
                    if(self->on_complete_) self->on_complete_(status?get_last_error():resval());
                    delete req;
                });
            }

        private:
            virtual stream* accept_new_()
            {
                auto x = new pipe;
                assert(x);

                int r = uv_accept(reinterpret_cast<uv_stream_t*>(&pipe_), reinterpret_cast<uv_stream_t*>(&x->pipe_));
                assert(r == 0);

                return x;
            }

        private:
            uv_pipe_t pipe_;
        };
    }
}

#endif
