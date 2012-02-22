#ifndef __DETAIL_FS_H__
#define __DETAIL_FS_H__

#include "base.h"

namespace native
{
    namespace detail
    {
        // example:
        //      fs::exec<fs::open, true>(callback, path.c_str(), flags, mode);
        //          execute uv_fs_open() asynchronously.
        //      fs::exec<fs::close, false>(callback, fd);
        //          execute uv_fs_close() synchronously.
        namespace fs
        {
            typedef std::function<void()> callback_type1;
            typedef std::function<void(resval)> callback_type2;
            typedef std::function<void(resval, int)> callback_type3;

            typedef void (*response_fn_type)(uv_fs_t*);

            void response_fn1(uv_fs_t* req)
            {
                event_emitter::invoke<callback_type1>(req->data);
            }

            void response_fn2(uv_fs_t* req)
            {
                if(req->result == -1)
                { event_emitter::invoke<callback_type2>(req->data, resval(static_cast<uv_err_code>(req->errorno))); }
                else
                { event_emitter::invoke<callback_type2>(req->data, resval()); }
            }

            void response_fn3(uv_fs_t* req)
            {
                if(req->result == -1)
                { event_emitter::invoke<callback_type3>(req->data, resval(static_cast<uv_err_code>(req->errorno)), -1); }
                else
                { event_emitter::invoke<callback_type3>(req->data, resval(), static_cast<int>(req->result)); }
            }

            template<int fstype, typename reqfntype, reqfntype* reqfn>
            struct type1
            {
                static const int fs_type = fstype;
                typedef callback_type1 callback_type;
                static constexpr reqfntype* request_fn = reqfn;
                static constexpr response_fn_type response_fn = &response_fn1;
            };

            template<int fstype, typename reqfntype, reqfntype* reqfn>
            struct type2
            {
                static const int fs_type = fstype;
                typedef callback_type2 callback_type;
                static constexpr reqfntype* request_fn = reqfn;
                static constexpr response_fn_type response_fn = &response_fn2;
            };

            template<int fstype, typename reqfntype, reqfntype* reqfn>
            struct type3
            {
                static const int fs_type = fstype;
                typedef callback_type3 callback_type;
                static constexpr reqfntype* request_fn = reqfn;
                static constexpr response_fn_type response_fn = &response_fn3;
            };

            struct utime : public type1<UV_FS_UTIME, decltype(uv_fs_utime), uv_fs_utime> {};
            struct futime : public type1<UV_FS_FUTIME, decltype(uv_fs_futime), uv_fs_futime> {};

            struct close : public type2<UV_FS_CLOSE, decltype(uv_fs_close), uv_fs_close> {};
            struct rename : public type2<UV_FS_RENAME, decltype(uv_fs_rename), uv_fs_rename> {};
            struct unlink : public type2<UV_FS_UNLINK, decltype(uv_fs_unlink), uv_fs_unlink> {};
            struct rmdir : public type2<UV_FS_RMDIR, decltype(uv_fs_rmdir), uv_fs_rmdir> {};
            struct mkdir : public type2<UV_FS_MKDIR, decltype(uv_fs_mkdir), uv_fs_mkdir> {};
            // TODO: implement fs::ftruncate
            //struct ftruncate : public type2<UV_FS_FTRUNCATE, decltype(uv_fs_ftruncate), uv_fs_ftruncate> {};
            struct fsync : public type2<UV_FS_FSYNC, decltype(uv_fs_fsync), uv_fs_fsync> {};
            struct fdatasync : public type2<UV_FS_FDATASYNC, decltype(uv_fs_fdatasync), uv_fs_fdatasync> {};
            struct link : public type2<UV_FS_LINK, decltype(uv_fs_link), uv_fs_link> {};
            struct symlink : public type2<UV_FS_SYMLINK, decltype(uv_fs_symlink), uv_fs_symlink> {};
            struct chmod : public type2<UV_FS_CHMOD, decltype(uv_fs_chmod), uv_fs_chmod> {};
            struct fchmod : public type2<UV_FS_FCHMOD, decltype(uv_fs_fchmod), uv_fs_fchmod> {};
            struct chown : public type2<UV_FS_CHOWN, decltype(uv_fs_chown), uv_fs_chown> {};
            struct fchown : public type2<UV_FS_FCHOWN, decltype(uv_fs_fchown), uv_fs_fchown> {};

            struct open : public type3<UV_FS_OPEN, decltype(uv_fs_open), uv_fs_open> {};
            struct sendfile : public type3<UV_FS_SENDFILE, decltype(uv_fs_sendfile), uv_fs_sendfile> {};
            struct read : public type3<UV_FS_READ, decltype(uv_fs_read), uv_fs_read> {};
            struct write : public type3<UV_FS_WRITE, decltype(uv_fs_write), uv_fs_write> {};

            // TODO: implement fs::stat, fs::lstat, fs::fstat
            struct stat {};
            struct lstat {};
            struct fstat {};

            struct readlink
            {
                static const int fs_type = UV_FS_READLINK;
                static constexpr decltype(&uv_fs_readlink) request_fn = &uv_fs_readlink;
                typedef std::function<void(resval, const std::string&)> callback_type;

                void response_fn(uv_fs_t* req)
                {
                    if(req->result == -1)
                    { event_emitter::invoke<callback_type>(req->data, resval(static_cast<uv_err_code>(req->errorno)), std::string()); }
                    else
                    { event_emitter::invoke<callback_type>(req->data, resval(), std::string(static_cast<char*>(req->ptr))); }
                }
            };

            struct readdir
            {
                static const int fs_type = UV_FS_READDIR;
                static constexpr decltype(&uv_fs_readdir) request_fn = &uv_fs_readdir;
                typedef std::function<void(resval, const std::vector<std::string>&)> callback_type;

                void response_fn(uv_fs_t* req)
                {
                    if(req->result == -1)
                    {
                        event_emitter::invoke<callback_type>(req->data, resval(static_cast<uv_err_code>(req->errorno)), std::vector<std::string>());
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

                        event_emitter::invoke<callback_type>(req->data, resval(), names);
                    }
                }
            };

            template<typename T>
            void exec_after_(uv_fs_t* req)
            {
                assert(req);
                assert(T::fs_type == req->fs_type);

                T::response_fn(req);

                // cleanup
                delete reinterpret_cast<event_emitter*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            }

            template<typename T, bool async, typename ...P>
            resval exec(typename T::callback_type callback, P&&... params)
            {
                auto req = new uv_fs_t;
                assert(req);

                req->data = new event_emitter();
                assert(req->data);
                event_emitter::add<typename T::callback_type>(req->data, callback);

                auto res = T::request_fn(uv_default_loop(), req, std::forward<P>(params)..., async?exec_after_<T>:nullptr);

                // async failure or sync success: invoke callback, clean up, and return error/success.
                if((async && res < 0) || (!async && res >= 0))
                {
                    resval rv = res < 0 ? resval(get_last_error()) : resval();

                    req->result = res;
                    req->path = nullptr;
                    req->errorno = rv.code();
                    exec_after_<T>(req);

                    return rv;
                }

                // sync failure: clean up and return error
                if(!async && res < 0)
                {
                    uv_fs_req_cleanup(req);
                    delete req;
                    return get_last_error();
                }

                // async success: return success (callback will be invoked later)
                return resval();
            }

            typedef std::function<void(const char*, std::size_t, resval)> rte_callback_type;

            class rte_context
            {
            public:
                rte_context(int fd, std::size_t buflen, rte_callback_type callback)
                    : fd_(fd)
                    , req_()
                    , buf_(buflen)
                    , result_()
                    , callback_(callback)
                {
                    req_.data = this;
                }

                ~rte_context()
                {}

                resval read(bool invoke_error)
                {
                    resval rv;
                    if(uv_fs_read(uv_default_loop(), &req_, fd_, &buf_[0], buf_.size(), result_.size(), rte_cb) < 0)
                    {
                        rv = get_last_error();
                        if(invoke_error)
                        {
                            end_error(rv);
                        }
                        else
                        {
                            uv_fs_req_cleanup(&req_);
                            delete this;
                        }
                    }
                    return rv;
                }

                resval read_more(std::size_t length)
                {
                    result_.insert(result_.end(), buf_.begin(), buf_.begin()+length);

                    uv_fs_req_cleanup(&req_);

                    return read(true);
                }

                void end_error(resval e)
                {
                    try
                    {
                        callback_(nullptr, 0, e);
                    }
                    catch(...)
                    {
                        // TODO: handle exception
                    }

                    uv_fs_req_cleanup(&req_);
                    delete this;
                }

                void end()
                {
                    try
                    {
                        callback_(&result_[0], result_.size(), resval());
                    }
                    catch(...)
                    {
                        // TODO: handle exception
                    }

                    uv_fs_req_cleanup(&req_);
                    delete this;
                }

            private:
                static void rte_cb(uv_fs_t* req)
                {
                    assert(req);
                    assert(req->fs_type == UV_FS_READ);

                    auto self = reinterpret_cast<rte_context*>(req->data);
                    assert(self);

                    if(req->errorno)
                    {
                        // error
                        self->end_error(resval(static_cast<uv_err_code>(req->errorno)));
                    }
                    else if(req->result == 0)
                    {
                        // EOF
                        self->end();
                    }
                    else
                    {
                        // continue reading
                        self->read_more(req->result);
                    }
                }

            private:
                int fd_;
                uv_fs_t req_;
                std::vector<char> buf_;
                std::vector<char> result_;
                rte_callback_type callback_;
            };

            // read all data asynchronously
            resval read_to_end(int fd, rte_callback_type callback)
            {
                auto ctx = new rte_context(fd, 512, callback);
                assert(ctx);
                return ctx->read(false);
            }
        }
    }
}

#endif
