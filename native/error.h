#ifndef __ERROR_H__
#define __ERROR_H__

#include"base.h"

namespace native
{
    class exception
    {
    public:
        exception(const std::string& message)
            : message_(message)
        {}

        virtual ~exception() {}

        const std::string& message() const { return message_; }

    private:
        std::string message_;
    };

    class error
    {
    public:
        error(uv_err_t e) : uv_err_(e) {}
        ~error() = default;

    public:
        const char* name() const { return uv_err_name(uv_err_); }
        const char* str() const { return uv_strerror(uv_err_); }

    private:
        uv_err_t uv_err_;
    };

	error get_last_error() { return uv_last_error(uv_default_loop()); }
}


#endif
