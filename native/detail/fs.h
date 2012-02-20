#ifndef __DETAIL_FS_H__
#define __DETAIL_FS_H__

#include "base.h"

namespace native
{
    namespace detail
    {
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
                {
                    callbacks::invoke<std::function<void(error, const std::string&)>>(req->data, 0, error(static_cast<uv_err_code>(req->errorno)), std::string());
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
                return get_last_error();
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
                return get_last_error();
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
                return get_last_error();
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
                return get_last_error();
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
                return get_last_error();
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
                return get_last_error();
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
    }
}

#endif
