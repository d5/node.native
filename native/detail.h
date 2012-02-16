#ifndef __DETAIL_H__
#define __DETAIL_H__

#include "base.h"
#include "utility.h"
#include <stdexcept>
#include <uv.h>
#include <http_parser.h>

namespace native
{
    namespace detail
    {
        enum
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

        int run()
        {
            return uv_run(uv_default_loop());
        }

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
                // TODO: isolate an exception caught in the callback.
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
            object(int max_cid)
                : lut_(new callbacks(max_cid))
            {}

            virtual ~object()
            {
                // TODO: make sure that lut_ is deleted properly and exactly once.
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
                : object(uv_cid_max)
                , handle_(handle)
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
                buf.len = static_cast<size_t>(length);

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

        static void on_fs_after(uv_fs_t* req)
        {
            switch(req->fs_type)
            {
            case UV_FS_CLOSE:
            case UV_FS_RENAME:
            case UV_FS_UNLINK:
            case UV_FS_RMDIR:
            case UV_FS_MKDIR:
            case UV_FS_FTRUNCATE:
            case UV_FS_FSYNC:
            case UV_FS_FDATASYNC:
            case UV_FS_LINK:
            case UV_FS_SYMLINK:
            case UV_FS_CHMOD:
            case UV_FS_FCHMOD:
            case UV_FS_CHOWN:
            case UV_FS_FCHOWN:
                if(req->result == -1)
                {
                    callbacks::invoke<std::function<void(error)>>(req->data, 0, error(static_cast<uv_err_code>(req->errorno)));
                }
                else
                {
                    callbacks::invoke<std::function<void(error)>>(req->data, 0, error());
                }
                break;

            case UV_FS_UTIME:
            case UV_FS_FUTIME:
                callbacks::invoke<std::function<void()>>(req->data, 0);
                break;

            case UV_FS_OPEN:
            case UV_FS_SENDFILE:
            case UV_FS_WRITE:
            case UV_FS_READ:
                if(req->result == -1)
                {
                    callbacks::invoke<std::function<void(error, int)>>(req->data, 0, error(static_cast<uv_err_code>(req->errorno)), -1);
                }
                else
                {
                    callbacks::invoke<std::function<void(error, int)>>(req->data, 0, error(), req->result);
                }
                break;

            case UV_FS_STAT:
            case UV_FS_LSTAT:
            case UV_FS_FSTAT:
                // TODO: implement UV_FS_*STAT types
                assert(false);
                break;

            case UV_FS_READLINK:
                if(req->result == -1)
                {                    callbacks::invoke<std::function<void(error, const std::string&)>>(req->data, 0, error(static_cast<uv_err_code>(req->errorno)), std::string());
                }
                else
                {
                    callbacks::invoke<std::function<void(error, const std::string&)>>(req->data, 0, error(), std::string(static_cast<char*>(req->ptr)));
                }
                break;

            case UV_FS_READDIR:
                if(req->result == -1)
                {
                    callbacks::invoke<std::function<void(error, const std::vector<std::string>&)>>(req->data, 0, error(static_cast<uv_err_code>(req->errorno)), std::vector<std::string>());
                }
                else
                {
                    char *namebuf = static_cast<char*>(req->ptr);
                    int nnames = req->result;

                    std::vector<std::string> names;

                    for(int i = 0; i < nnames; i++)
                    {
                        std::string name(namebuf);
                        names.push_back(name);

#ifndef NDEBUG
                        namebuf += name.length();
                        assert(*namebuf == '\0');
                        namebuf += 1;
#else
                        namebuf += name.length() + 1;
#endif
                    }

                    callbacks::invoke<std::function<void(error, const std::vector<std::string>&)>>(req->data, 0, error(), names);
                }
                break;

            default:
                assert(false);
                break;
            }

            // cleanup
            delete reinterpret_cast<callbacks*>(req->data);
            uv_fs_req_cleanup(req);
            delete req;
        }

        // NOTE: Callback function is always invoked even when failed to initiat uv_functions.
        //       However, when failed, callback function is invoked synchronously.
        // F: function name
        // P: parameters
        // C: callback signature
        template<typename F, typename C, typename ...P>
        error fs_exec_async(F uv_function, C callback, P&&... params)
        {
            auto req = new uv_fs_t;
            assert(req);

            req->data = new callbacks(1);
            assert(req->data);
            callbacks::store(req->data, 0, callback);

            auto res = uv_function(uv_default_loop(), req, std::forward<P>(params)..., on_fs_after);
            if(res < 0)
            {
                // failed: invoke callback function synchronously.
                req->result = res;
                req->path = nullptr;
                req->errorno = uv_last_error(uv_default_loop()).code;
                on_fs_after(req);
            }

            return error();
        }

        // TODO: Node.js implementation takes 'path' argument and pass it to exception if there's any.
        // F: function name
        // P: parameters
        template<typename F, typename ...P>
        error fs_exec_sync(F uv_function, P&&... params)
        {
            uv_fs_t req;

            auto res = uv_function(uv_default_loop(), &req, std::forward<P>(params)..., nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(&req);
                return error(uv_last_error(uv_default_loop()));
            }

            uv_fs_req_cleanup(&req);
            return error();
        }

        template<typename F, typename C, typename ...P>
        error fs_exec(F uv_function, C callback, P&&... params)
        {
            if(callback) return fs_exec_async(uv_function, callback, std::forward<P>(params)...);
            else return fs_exec_sync(uv_function, std::forward<P>(params)...);
        }

        error fs_close(int fd, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_close, callback, fd);
        }

        error fs_sym_link(const std::string& dest, const std::string& src, bool dir=false, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_symlink, callback, dest.c_str(), src.c_str(), dir?UV_FS_SYMLINK_DIR:0);
        }

        error fs_link(const std::string& orig_path, const std::string& new_path, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_link, callback, orig_path.c_str(), new_path.c_str());
        }

        error fs_read_link(const std::string& path, std::function<void(error, const std::string&)> callback)
        {
            return fs_exec_async(uv_fs_readlink, callback, path.c_str());
        }

        error fs_read_link_sync(const std::string& path, std::string& result)
        {
            uv_fs_t req;

            auto res = uv_fs_readlink(uv_default_loop(), &req, path.c_str(), nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(&req);
                return error(uv_last_error(uv_default_loop()));
            }

            result = std::string(static_cast<char*>(req.ptr));

            uv_fs_req_cleanup(&req);
            return error();
        }

        error fs_rename(const std::string& old_path, const std::string& new_path, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_rename, callback, old_path.c_str(), new_path.c_str());
        }

        error fs_fdata_sync(int fd, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_fdatasync, callback, fd);
        }

        error fs_fsync(int fd, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_fsync, callback, fd);
        }

        error fs_unlink(const std::string& path, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_unlink, callback, path.c_str());
        }

        error fs_rmdir(const std::string& path, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_rmdir, callback, path.c_str());
        }

        error fs_mkdir(const std::string& path, int mode, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_mkdir, callback, path.c_str(), mode);
        }

        error fs_sendfile(int out_fd, int in_fd, off_t in_offset, size_t length, std::function<void(error, int)> callback=nullptr)
        {
            return fs_exec(uv_fs_sendfile, callback, out_fd, in_fd, in_offset, length);
        }

        error fs_read_dir(const std::string& path, std::function<void(error, const std::vector<std::string>&)> callback)
        {
            return fs_exec_async(uv_fs_readdir, callback, path.c_str(), 0);
        }

        error fs_read_dir_sync(const std::string& path, std::vector<std::string>& names)
        {
            uv_fs_t req;

            auto res = uv_fs_readdir(uv_default_loop(), &req, path.c_str(), 0, nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(&req);
                return error(uv_last_error(uv_default_loop()));
            }

            char* namebuf = static_cast<char*>(req.ptr);
            int nnames = req.result;

            for(int i=0; i<nnames; i++)
            {
                std::string name(namebuf);
                names.push_back(name);

#ifndef NDEBUG
                namebuf += name.length();
                assert(*namebuf == '\0');
                namebuf += 1;
#else
                namebuf += name.length() + 1;
#endif
            }

            uv_fs_req_cleanup(&req);
            return error();
        }

        error fs_open(const std::string& path, int flags, int mode, std::function<void(error, int)> callback)
        {
            return fs_exec_async(uv_fs_open, callback, path.c_str(), flags, mode);
        }

        error fs_open_sync(const std::string& path, int flags, int mode, int& fd)
        {
            uv_fs_t req;

            auto res = uv_fs_readdir(uv_default_loop(), &req, path.c_str(), 0, nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(&req);
                return error(uv_last_error(uv_default_loop()));
            }

            fd = res;

            uv_fs_req_cleanup(&req);
            return error();
        }

        error fs_write(int fd, const std::vector<char>& buffer, off_t offset, size_t length, off_t position, std::function<void(error, int)> callback)
        {
            assert(offset < buffer.size());
            assert(offset + length <= buffer.size());

            return fs_exec_async(uv_fs_write, callback, fd, const_cast<char*>(&buffer[offset]), length, position);
        }

        error fs_write(int fd, const std::vector<char>& buffer, off_t offset, size_t length, std::function<void(error, int)> callback)
        {
            return fs_write(fd, buffer, offset, length, static_cast<off_t>(-1), callback);
        }

        error fs_write_sync(int fd, const std::vector<char>& buffer, off_t offset, size_t length, off_t position, size_t& nwritten)
        {
            assert(offset < buffer.size());
            assert(offset + length <= buffer.size());

            uv_fs_t req;

            auto res = uv_fs_write(uv_default_loop(), &req, fd, const_cast<char*>(&buffer[offset]), length, position, nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(&req);
                return error(uv_last_error(uv_default_loop()));
            }

            nwritten = static_cast<size_t>(res);

            uv_fs_req_cleanup(&req);
            return error();
        }

        error fs_write_sync(int fd, const std::vector<char>& buffer, off_t offset, size_t length, size_t& nwritten)
        {
            return fs_write_sync(fd, buffer, offset, length, static_cast<off_t>(-1), nwritten);
        }

        error fs_read(int fd, const std::vector<char>& buffer, off_t offset, size_t length, off_t position, std::function<void(error, int)> callback)
        {
            assert(offset < buffer.size());
            assert(offset + length <= buffer.size());

            return fs_exec_async(uv_fs_read, callback, fd, const_cast<char*>(&buffer[offset]), length, position);
        }

        error fs_read(int fd, const std::vector<char>& buffer, off_t offset, size_t length, std::function<void(error, int)> callback)
        {
            return fs_read(fd, buffer, offset, length, static_cast<off_t>(-1), callback);
        }

        error fs_read_sync(int fd, const std::vector<char>& buffer, off_t offset, size_t length, off_t position, size_t& nread)
        {
            assert(offset < buffer.size());
            assert(offset + length <= buffer.size());

            uv_fs_t req;

            auto res = uv_fs_read(uv_default_loop(), &req, fd, const_cast<char*>(&buffer[offset]), length, position, nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(&req);
                return error(uv_last_error(uv_default_loop()));
            }

            nread = static_cast<size_t>(res);

            uv_fs_req_cleanup(&req);
            return error();
        }

        error fs_read_sync(int fd, const std::vector<char>& buffer, off_t offset, size_t length, size_t& nread)
        {
            return fs_read_sync(fd, buffer, offset, length, static_cast<off_t>(-1), nread);
        }

        error fs_chmod(const std::string& path, int mode, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_chmod, callback, path.c_str(), mode);
        }

        error fs_fchmod(int fd, int mode, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_fchmod, callback, fd, mode);
        }

        error fs_chown(const std::string& path, int uid, int gid, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_chown, callback, path.c_str(), uid, gid);
        }

        error fs_fchfown(int fd, int uid, int gid, std::function<void(error)> callback=nullptr)
        {
            return fs_exec(uv_fs_fchown, callback, fd, uid, gid);
        }

        error fs_utime(const std::string& path, double atime, double mtime, std::function<void()> callback=nullptr)
        {
            return fs_exec(uv_fs_utime, callback, path.c_str(), atime, mtime);
        }

        error fs_futime(int fd, double atime, double mtime, std::function<void()> callback=nullptr)
        {
            return fs_exec(uv_fs_futime, callback, fd, atime, mtime);
        }

        // TODO: implement fs_*stat() functions.
        error fs_stat() { return error(); }
        error fs_lstat() { return error(); }
        error fs_fstat() { return error(); }

        // TODO: implement fs_truncate() function.
        error fs_truncate() { return error(); }

        class http_parse_result;
        bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

        class http_parse_result
        {
            friend bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

            typedef std::map<std::string, std::string, native::util::ci_less> headers_type;

        public:
            class url_obj
            {
            public:
                url_obj(const char* buffer, std::size_t length, bool is_connect=false)
                    : handle_(), buf_(buffer, length)
                {
                    int r = http_parser_parse_url(buffer, length, is_connect, &handle_);
                    if(r) throw r;
                }

                ~url_obj()
                {}

            public:
                std::string schema() const
                {
                    if(has_schema()) return buf_.substr(handle_.field_data[UF_SCHEMA].off, handle_.field_data[UF_SCHEMA].len);
                    return "HTTP";
                }

                std::string host() const
                {
                    // TODO: if not specified, use host name
                    if(has_schema()) return buf_.substr(handle_.field_data[UF_HOST].off, handle_.field_data[UF_HOST].len);
                    return std::string("localhost");
                }

                int port() const
                {
                    if(has_path()) return static_cast<int>(handle_.port);
                    return (schema() == "HTTP" ? 80 : 443);
                }

                std::string path() const
                {
                    if(has_path()) return buf_.substr(handle_.field_data[UF_PATH].off, handle_.field_data[UF_PATH].len);
                    return std::string("/");
                }

                std::string query() const
                {
                    if(has_query()) return buf_.substr(handle_.field_data[UF_QUERY].off, handle_.field_data[UF_QUERY].len);
                    return std::string();
                }

                std::string fragment() const
                {
                    if(has_query()) return buf_.substr(handle_.field_data[UF_FRAGMENT].off, handle_.field_data[UF_FRAGMENT].len);
                    return std::string();
                }

            private:
                bool has_schema() const { return handle_.field_set & (1<<UF_SCHEMA); }
                bool has_host() const { return handle_.field_set & (1<<UF_HOST); }
                bool has_port() const { return handle_.field_set & (1<<UF_PORT); }
                bool has_path() const { return handle_.field_set & (1<<UF_PATH); }
                bool has_query() const { return handle_.field_set & (1<<UF_QUERY); }
                bool has_fragment() const { return handle_.field_set & (1<<UF_FRAGMENT); }

            private:
                http_parser_url handle_;
                std::string buf_;
            };

        public:
            http_parse_result()
                : url_()
                , headers_()
            {}

            ~http_parse_result()
            {}

        public:
            const url_obj& url() const { return *url_; }
            const headers_type& headers() const { return headers_; }

        private:
            std::shared_ptr<url_obj> url_;
            headers_type headers_;
        };

        class http_parser_obj : public object
        {
            friend bool parse_http_request(stream&, std::function<void(const http_parse_result*, const char*)>);

        private:
            http_parser_obj()
                : object(1)
                , parser_()
                , was_header_value_(true)
                , last_header_field_()
                , last_header_value_()
                , settings_()
            {
            }

        public:
            ~http_parser_obj()
            {
            }

        private:
            http_parser parser_;
            http_parser_settings settings_;
            bool was_header_value_;
            std::string last_header_field_;
            std::string last_header_value_;

            http_parse_result result_;
        };

        /**
         *  @brief Parses HTTP request from the input stream.
         *
         *  @param input    Input stream.
         *  @param callback Callback funtion (or any compatible callbale object).
         *
         *  @remarks
         *      Callback function must have 2 parameters:
         *          - result: parsed result. If there was an error, this parameter is nullptr.
         *          - error: error message. If there was no error, this parameter is nullptr.
         *
         *      Note that this function is not non-blocking, meaning the callback function is invoked synchronously.
         */
        bool parse_http_request(stream& input, std::function<void(const http_parse_result*, const char*)> callback)
        {
            http_parser_obj obj;
            http_parser_init(&obj.parser_, HTTP_REQUEST);
            obj.parser_.data = &obj;

            callbacks::store(obj.lut(), 0, callback);

            obj.settings_.on_url = [](http_parser* parser, const char *at, size_t len) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                assert(at && len);
                try
                {
                    obj->result_.url_.reset(new http_parse_result::url_obj(at, len));
                    return 0;
                }
                catch(int e)
                {
                    callbacks::invoke<decltype(callback)>(obj->lut(), 0, nullptr, "Failed to parser URL in the request.");
                    return 1;
                }
            };
            obj.settings_.on_header_field = [](http_parser* parser, const char* at, size_t len) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                if(obj->was_header_value_)
                {
                    // new field started
                    if(!obj->last_header_field_.empty())
                    {
                        // add new entry
                        obj->result_.headers_[obj->last_header_field_] = obj->last_header_value_;
                        obj->last_header_value_.clear();
                    }

                    obj->last_header_field_ = std::string(at, len);
                    obj->was_header_value_ = false;
                }
                else
                {
                    // appending
                    obj->last_header_field_ += std::string(at, len);
                }
                return 0;
            };
            obj.settings_.on_header_value = [](http_parser* parser, const char* at, size_t len) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                if(!obj->was_header_value_)
                {
                    obj->last_header_value_ = std::string(at, len);
                    obj->was_header_value_ = true;
                }
                else
                {
                    // appending
                    obj->last_header_value_ += std::string(at, len);
                }
                return 0;
            };
            obj.settings_.on_headers_complete = [](http_parser* parser) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                // add last entry if any
                if(!obj->last_header_field_.empty())
                {
                    // add new entry
                    obj->result_.headers_[obj->last_header_field_] = obj->last_header_value_;
                }

                // TODO: implement body contents parsing
                // return 0;
                return 1; // do not parse body
            };
            obj.settings_.on_message_complete = [](http_parser* parser) {
                auto obj = reinterpret_cast<http_parser_obj*>(parser->data);
                assert(obj);

                // invoke callback
                callbacks::invoke<decltype(callback)>(obj->lut(), 0, &obj->result_, nullptr);
                return 0;
            };

            return false;
        }

        class sigslot_base
        {
        public:
            sigslot_base() {}
            virtual ~sigslot_base() {}
        public:
            virtual void add_callback(void*) = 0;
            virtual bool remove_callback(void*) = 0;
            virtual void reset() = 0;
        };

        template<typename>
        class sigslot;

        template<typename R, typename ...P>
        class sigslot<std::function<R(P...)>> : public sigslot_base
        {
        public:
            typedef std::function<R(P...)> callback_type;
            typedef std::shared_ptr<callback_type> callback_ptr;

        public:
            sigslot()
                : callbacks_()
            {}
            virtual ~sigslot()
            {}

        public:
            virtual void add_callback(void* callback)
            {
                callbacks_.insert(callback_ptr(reinterpret_cast<callback_type*>(callback)));
            }

            virtual bool remove_callback(void* callback)
            {
                return callbacks_.erase(callback_ptr(reinterpret_cast<callback_type*>(callback))) > 0;
            }

            virtual void reset()
            {
                callbacks_.clear();
            }

            void invoke(P&&... args)
            {
                for(auto c : callbacks_) (*c)(std::forward<P>(args)...);
            }

        private:
            std::set<callback_ptr> callbacks_;
        };

        class options
        {
        public:
            options()
                : map_()
            {}

            virtual ~options()
            {}

            const std::string& get(const std::string& name, const std::string& default_value=std::string()) const
            {
                auto it = map_.find(name);
                if(it != map_.end()) return it->second;
                else return default_value;
            }

            bool get(const std::string& name, std::string& value) const
            {
                auto it = map_.find(name);
                if(it == map_.end()) return false;
                value = it->second;
                return true;
            }

            std::string& operator[](const std::string& name)
            {
                return map_[name];
            }

            const std::string& operator[](const std::string& name) const
            {
                auto it = map_.find(name);
                if(it == map_.end()) throw std::out_of_range("No such element exists.");
                return it->second;
            }

        private:
            std::map<std::string, std::string> map_;
        };
    }
}

#endif
