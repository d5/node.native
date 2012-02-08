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

        enum open_flag
        {
            read_only = O_RDONLY,
            write_only = O_WRONLY,
            read_write = O_RDWR,
            append = O_APPEND,
            create = O_CREAT,
            excl = O_EXCL,
            truncate = O_TRUNC,
            no_follow = O_NOFOLLOW,
            directory = O_DIRECTORY,
            no_access_time = O_NOATIME,
            large_large = O_LARGEFILE,
        };

        template<typename callback_t> // void(file_handle fd, error e)
        bool open(const char* path, open_flag flags, int mode, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback);
            return uv_fs_open(uv_default_loop(), req, path, flags, mode, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_OPEN);

                if(req->errorno)
                {
                    // system error
                    callbacks::invoke<callback_t>(req->data, 0, file_handle(-1), error(uv_last_error(uv_default_loop())));
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

        template<typename callback_t> // void(const char* buf, int nread, error e)
        bool read(file_handle f, void* buf, size_t len, off_t offset, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback, buf);
            return uv_fs_read(uv_default_loop(), req, f, buf, len, offset, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_READ);

                if(req->errorno)
                {
                    // system error
                    callbacks::invoke<callback_t>(req->data, 0, nullptr, 0, error(uv_last_error(uv_default_loop())));
                }
                else if(req->result == 0)
                {
                    // EOF
                    callbacks::invoke<callback_t>(req->data, 0, nullptr, 0, error(UV_EOF));
                }
                else
                {
                    callbacks::invoke<callback_t>(req->data, 0,
                        reinterpret_cast<const char*>(callbacks::get_data<callback_t>(req->data, 0)),
                        req->result,
                        error());
                }

                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }

        namespace internal
        {
            struct rte_data
            {
                fs::file_handle file;

                const static int buflen = 1024;
                char buf[buflen];

                std::string result;
            };

            template<typename callback_t>
            void rte_cb(uv_fs_t* req)
            {
                assert(req->fs_type == UV_FS_READ);

                auto x = reinterpret_cast<rte_data*>(callbacks::get_data<callback_t>(req->data, 0));

                if(req->errorno)
                {
                    // system error
                    callbacks::invoke<callback_t>(req->data, 0, std::string(), error(uv_last_error(uv_default_loop())));

                    uv_fs_req_cleanup(req);
                    delete reinterpret_cast<callbacks*>(req->data);
                    delete req;
                }
                else if(req->result == 0)
                {
                    // EOF
                    callbacks::invoke<callback_t>(req->data, 0, x->result, error());

                    uv_fs_req_cleanup(req);
                    delete reinterpret_cast<callbacks*>(req->data);
                    delete req;
                }
                else
                {
                    x->result.append(std::string(x->buf, req->result));

                    uv_fs_req_cleanup(req);

                    if(uv_fs_read(uv_default_loop(), req, x->file, x->buf, rte_data::buflen, x->result.length(), rte_cb<callback_t>))
                    {
                        callbacks::invoke<callback_t>(req->data, 0, std::string(), error(uv_last_error(uv_default_loop())));

                        uv_fs_req_cleanup(req);
                        delete reinterpret_cast<callbacks*>(req->data);
                        delete req;
                    }
                }
            }
        }

        template<typename callback_t> // void(const std::string& str, error e)
        bool read_to_end(file_handle f, callback_t callback)
        {
            auto x = new internal::rte_data;
            x->file = f;

            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback, x);

            return uv_fs_read(uv_default_loop(), req, f, x->buf, internal::rte_data::buflen, 0, internal::rte_cb<callback_t>) == 0;
        }
    }

    class file
    {
    public:
        template<typename callback_t> // void(const std::string& str, error e)
        static bool read(const std::string& path, callback_t callback)
        {
            if(!fs::open(path.c_str(), fs::read_only, 0, [=](fs::file_handle f, error e) {
                if(e)
                {
                    callback(std::string(), e);
                }
                else
                {
                    if(!fs::read_to_end(f, [=](const std::string& str, error e){
                        callback(str, e);
                    }))
                    {
                        callback(std::string(), error(uv_last_error(uv_default_loop())));
                    }
                }
            })) return false;
        }

        template<typename callback_t>
        static bool write(const std::string& path, const std::string& str, callback_t callback)
        {
            return false;
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
