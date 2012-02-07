#ifndef __LOOP_H__
#define __LOOP_H__

#include "base.h"
#include "error.h"

namespace native
{
    class loop
    {
    public:
        loop(bool use_default=false)
            : uv_loop_(use_default ? uv_default_loop() : uv_loop_new())
        { }

        ~loop()
        {
            if(uv_loop_)
            {
                uv_loop_delete(uv_loop_);
                uv_loop_ = nullptr;
            }
        }

        uv_loop_t* get() { return uv_loop_; }

        bool run() { return uv_run(uv_loop_)==0; }
        bool run_once() { return uv_run_once(uv_loop_)==0; }

        void ref() { uv_ref(uv_loop_); }
        void unref() { uv_unref(uv_loop_); }
        void update_time() { uv_update_time(uv_loop_); }
        int64_t now() { return uv_now(uv_loop_); }

        error last_error() { return uv_last_error(uv_loop_); }

    private:
        loop(const loop&);
        void operator =(const loop&);

    private:
        uv_loop_t* uv_loop_;
    };

    int run()
    {
        return uv_run(uv_default_loop());
    }

    int run_once()
    {
        return uv_run(uv_default_loop());
    }
}


#endif
