#ifndef __FS_H__
#define __FS_H__

#include "base.h"
#include <functional>
#include <algorithm>
#include <fcntl.h>
#include "callback.h"

namespace native
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
        static const int no_access_time = O_NOATIME;
        static const int large_large = O_LARGEFILE;


        template<typename callback_t> // void(native::fs::file_handle fd, error e)
        bool open(const std::string& path, int flags, int mode, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback);
            return uv_fs_open(uv_default_loop(), req, path.c_str(), flags, mode, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_OPEN);

                if(req->errorno)
                {
                    callbacks::invoke<callback_t>(req->data, 0, file_handle(-1), error(req->errorno));
                }
                else if(req->result < 0)
                {
                    // failed to open
                    callbacks::invoke<callback_t>(req->data, 0, req->result, error(UV_ENOENT));
                }
                else
                {
                    callbacks::invoke<callback_t>(req->data, 0, req->result, error());
                }

                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }

        template<typename callback_t> // void(const std::string& str, error e)
        bool read(file_handle fd, size_t len, off_t offset, callback_t callback)
        {
            auto req = new uv_fs_t;
            auto buf = new char[len];
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback, buf);
            return uv_fs_read(uv_default_loop(), req, fd, buf, len, offset, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_READ);

                if(req->errorno)
                {
                    // system error
                    callbacks::invoke<callback_t>(req->data, 0, std::string(), error(req->errorno));
                }
                else if(req->result == 0)
                {
                    // EOF
                    callbacks::invoke<callback_t>(req->data, 0, std::string(), error(UV_EOF));
                }
                else
                {
                    callbacks::invoke<callback_t>(req->data, 0,
                        std::string(reinterpret_cast<const char*>(callbacks::get_data<callback_t>(req->data, 0)), req->result),
                        error());
                }

                delete[] reinterpret_cast<const char*>(callbacks::get_data<callback_t>(req->data, 0));
                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }

        template<typename callback_t> // void(int nwritten, error e)
        bool write(file_handle fd, const char* buf, size_t len, off_t offset, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback);

            // TODO: const_cast<> !!
            return uv_fs_write(uv_default_loop(), req, fd, const_cast<char*>(buf), len, offset, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_WRITE);

                if(req->errorno)
                {
                    // system error
                    callbacks::invoke<callback_t>(req->data, 0, 0, error(req->errorno));
                }
                else
                {

                    callbacks::invoke<callback_t>(req->data, 0, req->result, error());
                }

                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }

        namespace internal
        {
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
                    callbacks::invoke<callback_t>(req->data, 0, std::string(), error(req->errorno));

                    uv_fs_req_cleanup(req);
                    delete reinterpret_cast<rte_context*>(callbacks::get_data<callback_t>(req->data, 0));
                    delete reinterpret_cast<callbacks*>(req->data);
                    delete req;
                }
                else if(req->result == 0)
                {
                    // EOF
                    callbacks::invoke<callback_t>(req->data, 0, ctx->result, error());

                    uv_fs_req_cleanup(req);
                    delete reinterpret_cast<rte_context*>(callbacks::get_data<callback_t>(req->data, 0));
                    delete reinterpret_cast<callbacks*>(req->data);
                    delete req;
                }
                else
                {
                    ctx->result.append(std::string(ctx->buf, req->result));

                    uv_fs_req_cleanup(req);

                    if(uv_fs_read(uv_default_loop(), req, ctx->file, ctx->buf, rte_context::buflen, ctx->result.length(), rte_cb<callback_t>))
                    {
                        callbacks::invoke<callback_t>(req->data, 0, std::string(), error(uv_last_error(uv_default_loop())));

                        uv_fs_req_cleanup(req);
                        delete reinterpret_cast<rte_context*>(callbacks::get_data<callback_t>(req->data, 0));
                        delete reinterpret_cast<callbacks*>(req->data);
                        delete req;
                    }
                }
            }
        }

        template<typename callback_t> // void(const std::string& str, error e)
        bool read_to_end(file_handle fd, callback_t callback)
        {
            auto ctx = new internal::rte_context;
            ctx->file = fd;

            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback, ctx);

            return uv_fs_read(uv_default_loop(), req, fd, ctx->buf, internal::rte_context::buflen, 0, internal::rte_cb<callback_t>) == 0;
        }
    }

    class file
    {
    public:
        template<typename callback_t> // void(const std::string& str, error e)
        static bool read(const std::string& path, callback_t callback)
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

        template<typename callback_t> // void(int nwritten, error e)
        static bool write(const std::string& path, const std::string& str, callback_t callback)
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

        template<typename callback_t>
        static bool open(const std::string& path, callback_t callback)
        {
            return false;
        }

        template<typename callback_t>
        static bool unlink(const std::string& path, callback_t callback)
        {
            return false;
        }

    public:
        template<typename callback_t>
        bool close(callback_t callback)
        {
            return false;
        }
    };
}

#endif
