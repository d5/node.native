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

        Exception(detail::resval rv)
            : message_(rv.str())
        {}

        Exception(detail::resval rv, const std::string& message)
            : message_(message + "\n" + rv.str())
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
