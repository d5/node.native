#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "base.h"
#include "detail.h"

namespace native
{
    class process : public EventEmitter
    {
    public:
        static void nextTick(std::function<void()> callback)
        {
            detail::node::instance().add_tick_callback(callback);
        }
    };
}

#endif
