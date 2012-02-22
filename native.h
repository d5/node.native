#ifndef __NATIVE_H__
#define __NATIVE_H__

/**
 *  @mainpage Documentation
 *
 *  @section Introduction
 *  Project URL: http://nodenative.com
 *
 */

#include "native/base.h"
#include "native/detail.h"
#include "native/net.h"
#include "native/http.h"

/**
 *  All types of node.native are defined under native namespace.
 */
namespace native
{
    /**
     *  Starts the main loop with the callback.
     *
     *  @param callback     Callback object that is executed by node.native.
     *
     *  @return             This function always returns 0.
     */
    int run(std::function<void()> callback)
    {
        return detail::node::instance().start(callback);
    }
}

#endif
