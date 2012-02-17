#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "base.h"
#include "detail.h"
#include "utility.h"

namespace native
{
    class Object
    {
    public:
        Object()
            : props_()
        {}

        virtual ~Object()
        {}

    public:
        util::dict& props() { return props_; }
        const util::dict& props() const { return props_; }

    private:
        util::dict props_;
    };
}

#endif
