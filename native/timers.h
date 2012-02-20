#ifndef __TIMERS_H__
#define __TIMERS_H__

#include "base.h"
#include "detail.h"

namespace native
{
    class timers
    {
    public:
        static void active(void*) {}
        static void enroll(void*, unsigned int) {}
        static void unenroll(void*) {}
    };
}

#endif
