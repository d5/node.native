#ifndef __DETAIL_HANDLE_H__
#define __DETAIL_HANDLE_H__

#include "base.h"

namespace native
{
    namespace detail
    {
        class handle
        {
        protected:
            handle(uv_handle_t* handle)
                : handle_(handle)
                , unref_(false)
            {
                assert(handle_);
            }

            virtual ~handle()
            {
            }

        public:
            virtual void ref()
            {
                if(!unref_) return;
                unref_ = false;
                uv_ref(uv_default_loop());
            }

            virtual void unref()
            {
                if(unref_) return;
                unref_ = true;
                uv_unref(uv_default_loop());
            }

            virtual void set_handle(uv_handle_t* h)
            {
                handle_ = h;
                handle_->data = this;
            }

            // Note that the handle object itself is deleted in a deferred callback of uv_close() invoked in this function.
            virtual void close()
            {
                if(!handle_) return;

                uv_close(handle_, [](uv_handle_t* h) {
                    auto self = reinterpret_cast<handle*>(h->data);
                    assert(self && self->handle_ == nullptr);
                    delete self;
                });

                handle_ = nullptr;
                handle::ref();

                state_change();
            }

            virtual void state_change() {}

            uv_handle_t* uv_handle() { return handle_; }
            const uv_handle_t* uv_handle() const { return handle_; }

        private:
            uv_handle_t* handle_;
            bool unref_;
        };
    }
}

#endif
