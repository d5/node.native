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
    namespace internal
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
}

#endif
