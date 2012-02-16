#ifndef __PIPE_H__
#define __PIPE_H__

#include "base.h"
#include "events.h"
#include "stream.h"

namespace native
{
    namespace impl
    {
        enum uv_callback_id
        {
            uv_cid_close = 0,
            uv_cid_listen,
            uv_cid_read_start,
            uv_cid_read2_start,
            uv_cid_write,
            uv_cid_write2,
            uv_cid_shutdown,
            uv_cid_connect,
            uv_cid_connect6,
            uv_cid_max
        };

        class error
        {
        public:
            error() : err_() {} // default: no error
            error(uv_err_t e) : err_(e) {}
            error(uv_err_code code) : err_ {code, 0} {}
            ~error() {}

            operator bool() { return err_.code != UV_OK; }

            uv_err_code code() const { return err_.code; }
            const char* name() const { return uv_err_name(err_); }
            const char* str() const { return uv_strerror(err_); }
        private:
            uv_err_t err_;
        };

        sockaddr_in to_ip4_addr(const std::string& ip, int port) { return uv_ip4_addr(ip.c_str(), port); }
        sockaddr_in6 to_ip6_addr(const std::string& ip, int port) { return uv_ip6_addr(ip.c_str(), port); }

        error from_ip4_addr(sockaddr_in* src, std::string& ip, int& port)
        {
            char dest[16];
            if(uv_ip4_name(src, dest, 16) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin_port));
                return error();
            }
            return error(uv_last_error(uv_default_loop()));;
        }

        error from_ip6_addr(sockaddr_in6* src, std::string& ip, int& port)
        {
            char dest[46];
            if(uv_ip6_name(src, dest, 46) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin6_port));
                return error();
            }
            return error(uv_last_error(uv_default_loop()));;
        }

        error get_last_error()
        {
            return error(uv_last_error(uv_default_loop()));
        }

        class callback_object_base
        {
        public:
            callback_object_base(void* data)
                : data_(data)
            {
            }
            virtual ~callback_object_base()
            {
            }

            void* get_data() { return data_; }

        private:
            void* data_;
        };

        template<typename callback_t>
        class callback_object : public callback_object_base
        {
        public:
            callback_object(const callback_t& callback, void* data=nullptr)
                : callback_object_base(data)
                , callback_(callback)
            {
            }

            virtual ~callback_object()
            {
            }

        public:
            template<typename ...A>
            typename std::result_of<callback_t(A...)>::type invoke(A&& ... args)
            {
                return callback_(std::forward<A>(args)...);
            }

        private:
            callback_t callback_;
        };

        typedef std::shared_ptr<callback_object_base> callback_object_ptr;

        class callbacks
        {
        public:
            callbacks(int max_callbacks)
                : lut_(max_callbacks)
            {
            }
            ~callbacks()
            {
            }

            template<typename callback_t>
            static void store(void* target, int cid, const callback_t& callback, void* data=nullptr)
            {
                reinterpret_cast<callbacks*>(target)->lut_[cid] = callback_object_ptr(new callback_object<callback_t>(callback, data));
            }

            template<typename callback_t>
            static void* get_data(void* target, int cid)
            {
                return reinterpret_cast<callbacks*>(target)->lut_[cid]->get_data();
            }

            template<typename callback_t, typename ...A>
            static typename std::result_of<callback_t(A...)>::type invoke(void* target, int cid, A&& ... args)
            {
                auto x = dynamic_cast<callback_object<callback_t>*>(reinterpret_cast<callbacks*>(target)->lut_[cid].get());
                assert(x);
                return x->invoke(std::forward<A>(args)...);
            }

        private:
            std::vector<callback_object_ptr> lut_;
        };

        class object
        {
        public:
            object()
                : lut_(new callbacks(uv_cid_max))
            {}

            virtual ~object()
            {
                // TODO: test if callback lut is deleted properly and exactly once.
                if(lut_)
                {
                    delete lut_;
                    lut_ = nullptr;
                }
            }

        protected:
            callbacks* lut() { return lut_; }

        private:
            callbacks* lut_;
        };

        class handle : public object
        {
        public:
            handle(uv_handle_t* handle)
                : handle_(handle)
                , unref_(false)
            {
                assert(handle_);
                handle_->data = this;
            }

            virtual ~handle()
            {
            }

            void ref()
            {
                if(!unref_) return;
                unref_ = false;
                uv_ref(uv_default_loop());
            }

            void unref()
            {
                if(unref_) return;
                unref_ = true;
                uv_unref(uv_default_loop());
            }

            virtual void set_handle(uv_handle_t* h)
            {
                handle_ = h;
                handle_->data = this;
            }

            void close()
            {
                if(!handle_) return;

                uv_close(handle_, [](uv_handle_t* h){
                    auto self = reinterpret_cast<handle*>(h->data);
                    assert(self);
                    assert(!self->handle_);

                    delete self;
                });

                handle_ = nullptr;
                ref();

                state_change();
            }

            virtual void state_change() {}
        private:
            uv_handle_t* handle_;
            bool unref_;
        };

        class stream : public handle
        {
        public:
            stream(uv_stream_t* stream)
                : handle(reinterpret_cast<uv_handle_t*>(stream))
                , stream_(stream)
            {
                assert(stream_);
            }

            virtual ~stream()
            {}

            virtual void set_handle(uv_handle_t* h)
            {
                handle::set_handle(h);
                stream_ = reinterpret_cast<uv_stream_t*>(h);
            }

            void update_write_queue_size()
            {
                // TODO: stream::update_write_queue_size(). Do we need this function?
            }

            error read_start(std::function<void(const char*, int, int, error)> callback)
            {
                callbacks::store(lut(), uv_cid_read_start, callback);

                bool res = uv_read_start(stream_, on_alloc, [](uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
                    auto self = reinterpret_cast<stream*>(handle->data);
                    assert(self);

                    if(nread < 0)
                    {
                        callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_read_start, nullptr, 0, 0, error(uv_last_error(uv_default_loop())));
                    }
                    else
                    {
                        assert(nread <= buf.len);
                        if(nread > 0)
                        {
                            callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_read_start, buf.base, 0, static_cast<int>(nread), error());
                        }
                    }

                    if(buf.base) delete buf.base;
                }) == 0;

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error read2_start(std::function<void(const char*, int, int, uv_stream_t*)> callback)
            {
                assert(stream_->type == UV_NAMED_PIPE);
                assert(reinterpret_cast<uv_pipe_t*>(stream_)->ipc);

                callbacks::store(lut(), uv_cid_read2_start, callback);

                bool res = uv_read2_start(stream_, on_alloc, [](uv_pipe_t* handle, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
                    auto self = reinterpret_cast<stream*>(handle->data);
                    assert(self);

                    // TODO: implement read2_start callback function.
                    //callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_read2_start, buf.base, 0, static_cast<int>(nread), ???);

                    if(buf.base) delete buf.base;
                }) == 0;

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error read_stop()
            {
                bool res = uv_read_stop(stream_) == 0;

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            // TODO: Node.js implementation takes 4 parameter in callback function.
            error write(const char* data, int offset, int length, std::function<void(error)> callback)
            {
                auto req = new uv_write_t;
                assert(req);

                uv_buf_t buf;
                buf.base = const_cast<char*>(&data[offset]);
                buf.len = static_cast<std::size_t>(length);

                callbacks::store(lut(), uv_cid_write, callback);

                bool res = uv_write(req, stream_, &buf, 1, [](uv_write_t* req, int status){
                    auto self = reinterpret_cast<stream*>(req->handle->data);
                    assert(self);

                    self->update_write_queue_size();
                    callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_write, status?error(uv_last_error(uv_default_loop())):error());

                    delete req;
                });

                update_write_queue_size();

                if(!res) delete req;

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error write2()
            {
                assert(stream_->type == UV_NAMED_PIPE);
                assert(reinterpret_cast<uv_pipe_t*>(stream_)->ipc);

                // TODO: implement stream::write2() function.
                return error();
            }

            // TODO: Node.js implementation takes 3 parameter in callback function.
            error shutdown(std::function<void(error)> callback)
            {
                auto req = new uv_shutdown_t;
                assert(req);

                callbacks::store(lut(), uv_cid_shutdown, callback);

                bool res = uv_shutdown(req, stream_, [](uv_shutdown_t* req, int status){
                    auto self = reinterpret_cast<stream*>(req->handle->data);
                    assert(self);

                    callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_shutdown, status?error(uv_last_error(uv_default_loop())):error());

                    delete req;
                });

                if(!res) delete req;

                return res?error():error(uv_last_error(uv_default_loop()));
            }

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
        };

        class pipe : public stream
        {
        public:
            pipe(bool ipc=false)
                : stream(reinterpret_cast<uv_stream_t*>(&pipe_))
                , pipe_()
            {
                int r = uv_pipe_init(uv_default_loop(), &pipe_, ipc?1:0);
                assert(r == 0);

                update_write_queue_size();
            }

            virtual ~pipe()
            {}

        public:
            error bind(const std::string& name)
            {
                bool res = uv_pipe_bind(&pipe_, name.c_str());

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error listen(int backlog, std::function<void(std::shared_ptr<pipe>, error)> callback)
            {
                callbacks::store(lut(), uv_cid_listen, callback);
                bool res = uv_listen(reinterpret_cast<uv_stream_t*>(&pipe_), backlog, [](uv_stream_t* handle, int status) {
                    auto self = reinterpret_cast<pipe*>(handle->data);
                    assert(self);

                    assert(status == 0);

                    // TODO: what about 'ipc' parameter in ctor?
                    auto client_obj = std::shared_ptr<pipe>(new pipe);
                    assert(client_obj);

                    int r = uv_accept(handle, reinterpret_cast<uv_stream_t*>(&client_obj->pipe_));
                    assert(r == 0);

                    callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_listen, client_obj, error());
                });

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            void open(int fd)
            {
                uv_pipe_open(&pipe_, fd);
            }

            // TODO: Node.js implementation takes 5 parameter in callback function.
            void connect(const std::string& name, std::function<void(error, bool, bool)> callback)
            {
                auto req = new uv_connect_t;
                assert(req);

                callbacks::store(lut(), uv_cid_connect, callback);

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

                    callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_connect, status?error(uv_last_error(uv_default_loop())):error(), readable, writable);

                    delete req;
                });
            }
        private:
            uv_pipe_t pipe_;
        };

        class tcp : public stream
        {
        public:
            tcp()
                : stream(reinterpret_cast<uv_stream_t*>(&tcp_))
                , tcp_()
            {
                int r = uv_tcp_init(uv_default_loop(), &tcp_);
                assert(r == 0);

                update_write_queue_size();
            }

            virtual ~tcp()
            {}

        public:
            error get_sock_name(bool& is_ipv4, std::string& ip, int& port)
            {
                struct sockaddr_storage addr;
                int addrlen = static_cast<int>(sizeof(addr));
                bool res = uv_tcp_getsockname(&tcp_, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) == 0;

                if(res)
                {
                    assert(addr.ss_family == AF_INET || addr.ss_family == AF_INET6);

                    is_ipv4 = (addr.ss_family == AF_INET);
                    if(is_ipv4) return from_ip4_addr(reinterpret_cast<struct sockaddr_in*>(&addr), ip, port);
                    else return from_ip6_addr(reinterpret_cast<struct sockaddr_in6*>(&addr), ip, port);
                }
                else
                {
                    return error(uv_last_error(uv_default_loop()));
                }
            }

            error get_peer_name(bool& is_ipv4, std::string& ip, int& port)
            {
                struct sockaddr_storage addr;
                int addrlen = static_cast<int>(sizeof(addr));
                bool res = uv_tcp_getpeername(&tcp_, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) == 0;

                if(res)
                {
                    assert(addr.ss_family == AF_INET || addr.ss_family == AF_INET6);

                    is_ipv4 = (addr.ss_family == AF_INET);
                    if(is_ipv4) return from_ip4_addr(reinterpret_cast<struct sockaddr_in*>(&addr), ip, port);
                    else return from_ip6_addr(reinterpret_cast<struct sockaddr_in6*>(&addr), ip, port);
                }
                else
                {
                    return error(uv_last_error(uv_default_loop()));
                }
            }
            error set_no_delay(bool enable)
            {
                bool res = uv_tcp_nodelay(&tcp_, enable?1:0) == 0;
                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error set_keepalive(bool enable, unsigned int delay)
            {
                bool res = uv_tcp_keepalive(&tcp_, enable?1:0, delay) == 0;
                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error bind(const std::string& ip, int port)
            {
                struct sockaddr_in addr = to_ip4_addr(ip, port);

                bool res = uv_tcp_bind(&tcp_, addr) == 0;
                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error bind6(const std::string& ip, int port)
            {
                struct sockaddr_in6 addr = to_ip6_addr(ip, port);

                bool res = uv_tcp_bind6(&tcp_, addr) == 0;
                return res?error():error(uv_last_error(uv_default_loop()));
            }

            error listen(int backlog, std::function<void(std::shared_ptr<tcp>, error)> callback)
            {
                callbacks::store(lut(), uv_cid_listen, callback);
                bool res = uv_listen(reinterpret_cast<uv_stream_t*>(&tcp_), backlog, [](uv_stream_t* handle, int status) {
                    auto self = reinterpret_cast<tcp*>(handle->data);
                    assert(self);

                    if(status == 0)
                    {
                        auto client_obj = std::shared_ptr<tcp>(new tcp);
                        assert(client_obj);

                        int r = uv_accept(handle, reinterpret_cast<uv_stream_t*>(&client_obj->tcp_));
                        assert(r == 0);

                        callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_listen, client_obj, error());
                    }
                    else
                    {
                        callbacks::invoke<decltype(callback)>(self->lut(), uv_cid_listen, nullptr, error(uv_last_error(uv_default_loop())));
                    }
                });

                return res?error():error(uv_last_error(uv_default_loop()));
            }

            // TODO: Node.js implementation takes 5 parameter in callback function.
            error connect(const std::string& ip, int port, std::function<void(error, bool, bool)> callback)
            {
                struct sockaddr_in addr = to_ip4_addr(ip, port);

                auto req = new uv_connect_t;
                assert(req);

                callbacks::store(lut(), uv_cid_connect, callback);

                bool res = uv_tcp_connect(req, &tcp_, addr, on_connect) == 0;
                if(!res) delete req;
                return res?error():error(uv_last_error(uv_default_loop()));
            }

            // TODO: Node.js implementation takes 5 parameter in callback function.
            error connect6(const std::string& ip, int port, std::function<void(error, bool, bool)> callback)
            {
                struct sockaddr_in6 addr = to_ip6_addr(ip, port);

                auto req = new uv_connect_t;
                assert(req);

                callbacks::store(lut(), uv_cid_connect, callback);

                bool res = uv_tcp_connect6(req, &tcp_, addr, on_connect) == 0;
                if(!res) delete req;
                return res?error():error(uv_last_error(uv_default_loop()));
            }

        private:
            static void on_connect(uv_connect_t* req, int status)
            {
                auto self = reinterpret_cast<tcp*>(req->handle->data);
                assert(self);

                callbacks::invoke<std::function<void(error, bool, bool)>>(self->lut(), uv_cid_connect,
                    status?error(uv_last_error(uv_default_loop())):error(), true, true);

                delete req;
            }

        private:
            uv_tcp_t tcp_;
        };
    }
}

#endif
