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

        virtual ~Exception()
        {}

    private:
        std::string message_;
    };
}

#endif
