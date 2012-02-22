#ifndef __DETAIL_FS_H__
#define __DETAIL_FS_H__

#include "base.h"

namespace native
{
    namespace detail
    {
        class fs_context_
        {

        };

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
                    callbacks::invoke<std::function<void(resval)>>(req->data, 0, resval(static_cast<uv_err_code>(req->errorno)));
                }
                else
                {
                    callbacks::invoke<std::function<void(resval)>>(req->data, 0, resval());
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
                    callbacks::invoke<std::function<void(resval, int)>>(req->data, 0, resval(static_cast<uv_err_code>(req->errorno)), -1);
                }
                else
                {
                    callbacks::invoke<std::function<void(resval, int)>>(req->data, 0, resval(), req->result);
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
                {
                    callbacks::invoke<std::function<void(resval, const std::string&)>>(req->data, 0, resval(static_cast<uv_err_code>(req->errorno)), std::string());
                }
                else
                {
                    callbacks::invoke<std::function<void(resval, const std::string&)>>(req->data, 0, resval(), std::string(static_cast<char*>(req->ptr)));
                }
                break;

            case UV_FS_READDIR:
                if(req->result == -1)
                {
                    callbacks::invoke<std::function<void(resval, const std::vector<std::string>&)>>(req->data, 0, resval(static_cast<uv_err_code>(req->errorno)), std::vector<std::string>());
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

                    callbacks::invoke<std::function<void(resval, const std::vector<std::string>&)>>(req->data, 0, resval(), names);
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
        resval fs_exec_async(F uv_function, C callback, P&&... params)
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
                return resval(static_cast<uv_err_code>(req->errorno));
            }

            // return value is always no error.
            return resval();
        }

        // TODO: Node.js implementation takes 'path' argument and pass it to exception if there's any.
        // F: function name
        // P: parameters
        template<typename F, typename C, typename ...P>
        resval fs_exec_sync(F uv_function, C callback, P&&... params)
        {
            auto req = new uv_fs_t;
            assert(req);

            req->data = new callbacks(1);
            assert(req->data);
            callbacks::store(req->data, 0, callback);

            auto res = uv_function(uv_default_loop(), req, std::forward<P>(params)..., nullptr);
            if(res < 0)
            {
                uv_fs_req_cleanup(req);
                delete req;
                return get_last_error();
            }

            req->result = res;
            req->path = nullptr;
            req->errorno = 0;
            on_fs_after(req);
            return resval();
        }

        template<bool async, typename F, typename C, typename ...P>
        resval fs_exec(F uv_function, C callback, P&&... params)
        {
            if(async) return fs_exec_async(uv_function, callback, std::forward<P>(params)...);
            else return fs_exec_sync(uv_function, callback, std::forward<P>(params)...);
        }

        template<bool async=true>
        resval fs_close(int fd, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_close, callback, fd);
        }

        template<bool async=true>
        resval fs_sym_link(const std::string& dest, const std::string& src, bool dir, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_symlink, callback, dest.c_str(), src.c_str(), dir?UV_FS_SYMLINK_DIR:0);
        }

        template<bool async=true>
        resval fs_sym_link(const std::string& dest, const std::string& src, std::function<void(resval)> callback)
        {
            return fs_sym_link<async>(dest, src, false, callback);
        }

        template<bool async=true>
        resval fs_link(const std::string& orig_path, const std::string& new_path, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_link, callback, orig_path.c_str(), new_path.c_str());
        }

        template<bool async=true>
        resval fs_read_link(const std::string& path, std::function<void(resval, const std::string&)> callback)
        {
            return fs_exec<async>(uv_fs_readlink, callback, path.c_str());
        }

        template<bool async=true>
        resval fs_rename(const std::string& old_path, const std::string& new_path, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_rename, callback, old_path.c_str(), new_path.c_str());
        }

        template<bool async=true>
        resval fs_fdata_sync(int fd, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_fdatasync, callback, fd);
        }

        template<bool async=true>
        resval fs_fsync(int fd, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_fsync, callback, fd);
        }

        template<bool async=true>
        resval fs_unlink(const std::string& path, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_unlink, callback, path.c_str());
        }

        template<bool async=true>
        resval fs_rmdir(const std::string& path, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_rmdir, callback, path.c_str());
        }

        template<bool async=true>
        resval fs_mkdir(const std::string& path, int mode, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_mkdir, callback, path.c_str(), mode);
        }

        template<bool async=true>
        resval fs_sendfile(int out_fd, int in_fd, off_t in_offset, size_t length, std::function<void(resval, int)> callback)
        {
            return fs_exec<async>(uv_fs_sendfile, callback, out_fd, in_fd, in_offset, length);
        }

        template<bool async=true>
        resval fs_read_dir(const std::string& path, std::function<void(resval, const std::vector<std::string>&)> callback)
        {
            return fs_exec<async>(uv_fs_readdir, callback, path.c_str(), 0);
        }

        template<bool async=true>
        resval fs_open(const std::string& path, int flags, int mode, std::function<void(resval, int)> callback)
        {
            return fs_exec<async>(uv_fs_open, callback, path.c_str(), flags, mode);
        }

        template<bool async=true>
        resval fs_write(int fd, const std::vector<char>& buffer, off_t offset, size_t length, off_t position, std::function<void(resval, int)> callback)
        {
            assert(offset < buffer.size());
            assert(offset + length <= buffer.size());

            return fs_exec<async>(uv_fs_write, callback, fd, const_cast<char*>(&buffer[offset]), length, position);
        }

        template<bool async=true>
        resval fs_write(int fd, const std::vector<char>& buffer, off_t offset, size_t length, std::function<void(resval, int)> callback)
        {
            return fs_write<async>(fd, buffer, offset, length, static_cast<off_t>(-1), callback);
        }

        template<bool async=true>
        resval fs_read(int fd, const std::vector<char>& buffer, off_t offset, size_t length, off_t position, std::function<void(resval, int)> callback)
        {
            assert(offset < buffer.size());
            assert(offset + length <= buffer.size());

            return fs_exec<async>(uv_fs_read, callback, fd, const_cast<char*>(&buffer[offset]), length, position);
        }

        template<bool async=true>
        resval fs_read(int fd, const std::vector<char>& buffer, off_t offset, size_t length, std::function<void(resval, int)> callback)
        {
            return fs_read<async>(fd, buffer, offset, length, static_cast<off_t>(-1), callback);
        }

        template<bool async=true>
        resval fs_chmod(const std::string& path, int mode, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_chmod, callback, path.c_str(), mode);
        }

        template<bool async=true>
        resval fs_fchmod(int fd, int mode, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_fchmod, callback, fd, mode);
        }

        template<bool async=true>
        resval fs_chown(const std::string& path, int uid, int gid, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_chown, callback, path.c_str(), uid, gid);
        }

        template<bool async=true>
        resval fs_fchfown(int fd, int uid, int gid, std::function<void(resval)> callback)
        {
            return fs_exec<async>(uv_fs_fchown, callback, fd, uid, gid);
        }

        template<bool async=true>
        resval fs_utime(const std::string& path, double atime, double mtime, std::function<void()> callback)
        {
            return fs_exec<async>(uv_fs_utime, callback, path.c_str(), atime, mtime);
        }

        template<bool async=true>
        resval fs_futime(int fd, double atime, double mtime, std::function<void()> callback)
        {
            return fs_exec<async>(uv_fs_futime, callback, fd, atime, mtime);
        }

        // TODO: implement fs_*stat() functions.
        template<bool async=true>
        resval fs_stat() { return resval(); }
        template<bool async=true>
        resval fs_lstat() { return resval(); }
        template<bool async=true>
        resval fs_fstat() { return resval(); }

        // TODO: implement fs_truncate() function.
        template<bool async=true>
        resval fs_truncate() { return resval(); }

        // read all data asynchronously
        resval fs_read_to_end(int fd, rte_callback_type callback)
        {
            auto ctx = new rte_context(fd, 512, callback);
            assert(ctx);
            return ctx->read(false);
        }
    }
}

#endif
