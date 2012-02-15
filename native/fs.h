#ifndef __FS_H__
#define __FS_H__

#include "base.h"
#include <functional>
#include <algorithm>
#include <fcntl.h>
#include "callback.h"

namespace native
{
    namespace detail
    {
        // TODO: implement functions that accept loop pointer as extra argument.
        namespace fs
        {
            typedef uv_file file_handle;

            static const int read_only = O_RDONLY;
            static const int write_only = O_WRONLY;
            static const int read_write = O_RDWR;
            static const int append = O_APPEND;
            static const int create = O_CREAT;
            static const int excl = O_EXCL;
            static const int truncate = O_TRUNC;
            static const int no_follow = O_NOFOLLOW;
            static const int directory = O_DIRECTORY;
    #ifdef O_NOATIME
            static const int no_access_time = O_NOATIME;
    #endif
    #ifdef O_LARGEFILE
            static const int large_large = O_LARGEFILE;
    #endif

            namespace internal
            {
                template<typename callback_t>
                uv_fs_t* create_req(callback_t callback, void* data=nullptr)
                {
                    auto req = new uv_fs_t;
                    req->data = new callbacks(1);
                    assert(req->data);
                    callbacks::store(req->data, 0, callback, data);

                    return req;
                }

                template<typename callback_t, typename ...A>
                typename std::result_of<callback_t(A...)>::type invoke_from_req(uv_fs_t* req, A&& ... args)
                {
                    return callbacks::invoke<callback_t>(req->data, 0, std::forward<A>(args)...);
                }

                template<typename callback_t, typename data_t>
                data_t* get_data_from_req(uv_fs_t* req)
                {
                    return reinterpret_cast<data_t*>(callbacks::get_data<callback_t>(req->data, 0));
                }

                template<typename callback_t, typename data_t>
                void delete_req(uv_fs_t* req)
                {
                    delete reinterpret_cast<data_t*>(callbacks::get_data<callback_t>(req->data, 0));
                    delete reinterpret_cast<callbacks*>(req->data);
                    uv_fs_req_cleanup(req);
                    delete req;
                }

                template<typename callback_t, typename data_t>
                void delete_req_arr_data(uv_fs_t* req)
                {
                    delete[] reinterpret_cast<data_t*>(callbacks::get_data<callback_t>(req->data, 0));
                    delete reinterpret_cast<callbacks*>(req->data);
                    uv_fs_req_cleanup(req);
                    delete req;
                }

                void delete_req(uv_fs_t* req)
                {
                    delete reinterpret_cast<callbacks*>(req->data);
                    uv_fs_req_cleanup(req);
                    delete req;
                }

                struct rte_context
                {
                    fs::file_handle file;
                    const static int buflen = 32;
                    char buf[buflen];
                    std::string result;
                };

                template<typename callback_t>
                void rte_cb(uv_fs_t* req)
                {
                    assert(req->fs_type == UV_FS_READ);

                    auto ctx = reinterpret_cast<rte_context*>(callbacks::get_data<callback_t>(req->data, 0));
                    if(req->errorno)
                    {
                        // system error
                        invoke_from_req<callback_t>(req, std::string(), error(req->errorno));
                        delete_req<callback_t, rte_context>(req);
                    }
                    else if(req->result == 0)
                    {
                        // EOF
                        invoke_from_req<callback_t>(req, ctx->result, error());
                        delete_req<callback_t, rte_context>(req);
                    }
                    else
                    {
                        ctx->result.append(std::string(ctx->buf, req->result));

                        uv_fs_req_cleanup(req);

                        if(uv_fs_read(uv_default_loop(), req, ctx->file, ctx->buf, rte_context::buflen, ctx->result.length(), rte_cb<callback_t>))
                        {
                            // failed to initiate uv_fs_read():
                            invoke_from_req<callback_t>(req, std::string(), error(uv_last_error(uv_default_loop())));
                            delete_req<callback_t, rte_context>(req);
                        }
                    }
                }
            }

            bool open(const std::string& path, int flags, int mode, std::function<void(native::fs::file_handle fd, error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_open(uv_default_loop(), req, path.c_str(), flags, mode, [](uv_fs_t* req) {
                    assert(req->fs_type == UV_FS_OPEN);

                    if(req->errorno) internal::invoke_from_req<decltype(callback)>(req, file_handle(-1), error(req->errorno));
                    else internal::invoke_from_req<decltype(callback)>(req, req->result, error(req->result<0?UV_ENOENT:UV_OK));

                    internal::delete_req(req);
                })) {
                    // failed to initiate uv_fs_open()
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool read(file_handle fd, size_t len, off_t offset, std::function<void(const std::string& str, error e)> callback)
            {
                auto buf = new char[len];
                auto req = internal::create_req(callback, buf);
                if(uv_fs_read(uv_default_loop(), req, fd, buf, len, offset, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_READ);

                    if(req->errorno)
                    {
                        // system error
                        internal::invoke_from_req<decltype(callback)>(req, std::string(), error(req->errorno));
                    }
                    else if(req->result == 0)
                    {
                        // EOF
                        internal::invoke_from_req<decltype(callback)>(req, std::string(), error(UV_EOF));
                    }
                    else
                    {
                        auto buf = internal::get_data_from_req<decltype(callback), char>(req);
                        internal::invoke_from_req<decltype(callback)>(req, std::string(buf, req->result), error());
                    }

                    internal::delete_req_arr_data<decltype(callback), char>(req);
                })) {
                    // failed to initiate uv_fs_read()
                    internal::delete_req_arr_data<decltype(callback), char>(req);
                    return false;
                }
                return true;
            }

            bool write(file_handle fd, const char* buf, size_t len, off_t offset, std::function<void(int nwritten, error e)> callback)
            {
                auto req = internal::create_req(callback);

                // TODO: const_cast<> !!
                if(uv_fs_write(uv_default_loop(), req, fd, const_cast<char*>(buf), len, offset, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_WRITE);

                    if(req->errorno)
                    {
                        internal::invoke_from_req<decltype(callback)>(req, 0, error(req->errorno));
                    }
                    else
                    {
                        internal::invoke_from_req<decltype(callback)>(req, req->result, error());
                    }

                    internal::delete_req(req);
                })) {
                    // failed to initiate uv_fs_write()
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool read_to_end(file_handle fd, std::function<void(const std::string& str, error e)> callback)
            {
                auto ctx = new internal::rte_context;
                ctx->file = fd;
                auto req = internal::create_req(callback, ctx);

                if(uv_fs_read(uv_default_loop(), req, fd, ctx->buf, internal::rte_context::buflen, 0, internal::rte_cb<decltype(callback)>)) {
                    // failed to initiate uv_fs_read()
                    internal::delete_req<decltype(callback), internal::rte_context>(req);
                    return false;
                }
                return true;
            }

            bool close(file_handle fd, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_close(uv_default_loop(), req, fd, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_CLOSE);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool unlink(const std::string& path, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_unlink(uv_default_loop(), req, path.c_str(), [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_UNLINK);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool mkdir(const std::string& path, int mode, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_mkdir(uv_default_loop(), req, path.c_str(), mode, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_MKDIR);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool rmdir(const std::string& path, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_rmdir(uv_default_loop(), req, path.c_str(), [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_RMDIR);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool rename(const std::string& path, const std::string& new_path, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_rename(uv_default_loop(), req, path.c_str(), new_path.c_str(), [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_RENAME);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool chmod(const std::string& path, int mode, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_chmod(uv_default_loop(), req, path.c_str(), mode, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_CHMOD);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool chown(const std::string& path, int uid, int gid, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_chown(uv_default_loop(), req, path.c_str(), uid, gid, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_CHOWN);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

    #if 0
            bool readdir(const std::string& path, int flags, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_readdir(uv_default_loop(), req, path.c_str(), flags, [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_READDIR);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool stat(const std::string& path, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_stat(uv_default_loop(), req, path.c_str(), [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_STAT);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }

            bool fstat(const std::string& path, std::function<void(error e)> callback)
            {
                auto req = internal::create_req(callback);
                if(uv_fs_fstat(uv_default_loop(), req, path.c_str(), [](uv_fs_t* req){
                    assert(req->fs_type == UV_FS_FSTAT);
                    internal::invoke_from_req<decltype(callback)>(req, req->errorno?error(req->errorno):error());
                    internal::delete_req(req);
                })) {
                    internal::delete_req(req);
                    return false;
                }
                return true;
            }
    #endif
        }

        class file
        {
        public:
            static bool read(const std::string& path, std::function<void(const std::string& str, error e)> callback)
            {
                if(!fs::open(path.c_str(), fs::read_only, 0, [=](fs::file_handle fd, error e) {
                    if(e)
                    {
                        callback(std::string(), e);
                    }
                    else
                    {
                        if(!fs::read_to_end(fd, callback))
                        {
                            // failed to initiate read_to_end()
                            callback(std::string(), error(uv_last_error(uv_default_loop())));
                        }
                    }
                })) return false;
            }

            static bool write(const std::string& path, const std::string& str, std::function<void(int nwritten, error e)> callback)
            {
                if(!fs::open(path.c_str(), fs::write_only|fs::create, 0664, [=](fs::file_handle fd, error e) {
                    if(e)
                    {
                        callback(0, e);
                    }
                    else
                    {
                        if(!fs::write(fd, str.c_str(), str.length(), 0, callback))
                        {
                            // failed to initiate read_to_end()
                            callback(0, error(uv_last_error(uv_default_loop())));
                        }
                    }
                })) return false;
            }
        };
    }
}

#endif
