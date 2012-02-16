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

namespace native
{
    /**
     *  Runs the main loop.
     */
    int run()
    {
        return detail::run();
    }
}

#endif
