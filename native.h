#ifndef __NATIVE_H__
#define __NATIVE_H__

/**
 *  @mainpage Documentation
 *
 *  @section Introduction
 *  Project URL: https://github.com/d5/node.native
 *
 */

#include "native/base.h"
#include "native/detail.h"
#include "native/utility.h"
#include "native/net.h"

namespace native
{
    int run(std::function<void()> callback)
    {
        return detail::node::instance().start(callback);
    }
}

#endif
