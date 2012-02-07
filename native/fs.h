#ifndef __FS_H__
#define __FS_H__

#include "base.h"
#include "callback.h"

namespace native
{
    typedef uv_file file;

    // TODO: implement functions that accept loop pointer as extra argument.
    namespace fs
    {
        template<typename callback_t>
        bool open(const char* path, int flags, int mode, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback);
            return uv_fs_open(uv_default_loop(), req, path, flags, mode, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_OPEN);

                if(req->errorno || req->result < 0)
                {
                    // TODO: handle error
                }
                else
                {
                    callbacks::invoke<callback_t>(req->data, 0, req->result);
                }

                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }

        template<typename callback_t>
        bool read(file f, void* buf, size_t len, off_t offset, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback);
            return uv_fs_read(uv_default_loop(), req, f, buf, len, offset, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_READ);

                if(req->errorno)
                {
                    // TODO: handle error
                }
                else
                {
                    callbacks::invoke<callback_t>(req->data, 0, req);
                }

                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }

        template<typename callback_t>
        bool mkdir(const char* path, int mode, callback_t callback)
        {
            auto req = new uv_fs_t;
            req->data = new callbacks(1);
            callbacks::store(req->data, 0, callback);
            return uv_fs_mkdir(uv_default_loop(), req, path, mode, [](uv_fs_t* req){
                assert(req->fs_type == UV_FS_MKDIR);

                if(req->errorno)
                {
                    // TODO: handle error
                }
                else
                {
                    callbacks::invoke<callback_t>(req->data, 0, req->result);
                }

                delete reinterpret_cast<callbacks*>(req->data);
                uv_fs_req_cleanup(req);
                delete req;
            })==0;
        }
    }
}

#endif
