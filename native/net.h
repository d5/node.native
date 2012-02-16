#ifndef __NET_H__
#define __NET_H__

#include "base.h"
#include "detail.h"
#include <arpa/inet.h>

namespace native
{
    namespace net
    {
        /**
         *  @brief Gets the version of IP address format.
         *
         *  @param input    Input IP string.
         *
         *  @retval 4       Input string is a valid IPv4 address.
         *  @retval 6       Input string is a valid IPv6 address.
         *  @retval 0       Input string is not a valid IP address.
         */
        int isIP(const std::string& input)
        {
            if(input.empty()) return 0;

            unsigned char buf[sizeof(in6_addr)];

            if(inet_pton(AF_INET, input.c_str(), buf) == 1) return 4;
            else if(inet_pton(AF_INET6, input.c_str(), buf) == 1) return 6;

            return 0;
        }

        /**
         *  @brief Tests if the input string is valid IPv4 address.
         *
         *  @param input    Input IP string.
         *
         *  @retval true    Input string is a valid IPv4 address.
         *  @retval false    Input string is not a valid IPv4 address.
         */
        bool isIPv4(const std::string& input) { return isIP(input) == 4; }

        /**
         *  @brief Test if the input string is valid IPv6 address.
         *
         *  @param input    Input IP string.
         *
         *  @retval true    Input string is a valid IPv6 address.
         *  @retval false    Input string is not a valid IPv6 address.
         */
        bool isIPv6(const std::string& input) { return isIP(input) == 6; }
    }
}

#endif
