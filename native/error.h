#ifndef __ERROR_H__
#define __ERROR_H__

#include "base.h"
#include "detail.h"

namespace native
{
    class Exception
    {
    public:
        Exception(const std::string& message=std::string())
            : message_(message)
        {}

        Exception(detail::error e)
            : message_(e.str())
        {}

        Exception(detail::error e, const std::string& message)
            : message_(message + "\n" + e.str())
        {}

        Exception(std::nullptr_t)
            : message_()
        {}

        virtual ~Exception()
        {}

    public:
        const std::string& message() const { return message_; }

    private:
        std::string message_;
    };
}

#endif
