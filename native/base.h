#ifndef __BASE_H__
#define __BASE_H__

#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include <map>
#include <algorithm>
#include <list>
#include <set>
#include <tuple>

#include <uv.h>

namespace native
{
    namespace detail
    {
        enum uv_callback_id
        {
            uv_cid_close = 0,
            uv_cid_listen,
            uv_cid_read_start,
            uv_cid_write,
            uv_cid_shutdown,
            uv_cid_connect,
            uv_cid_connect6,
            uv_cid_max
        };
    }

    class Error
    {
    public:
        Error() : uv_err_() {}
        Error(uv_err_t e) : uv_err_(e) {}
        Error(uv_err_code c) : uv_err_{ c, 0 } {}
        Error(int c) : uv_err_{ static_cast<uv_err_code>(c), 0 } { }
        ~Error() = default;

    public:
        operator bool() { return uv_err_.code != UV_OK; }

        static Error fromStatusCode(int status)
        {
            return status?Error(uv_last_error(uv_default_loop())):Error();
        }

        static Error getLastError()
        {
            return Error(uv_last_error(uv_default_loop()));
        }

        uv_err_code code() const { return uv_err_.code; }
        const char* name() const { return uv_err_name(uv_err_); }
        const char* str() const { return uv_strerror(uv_err_); }

    private:
        uv_err_t uv_err_;
    };

    Error get_last_error() { return uv_last_error(uv_default_loop()); }
}

#endif
