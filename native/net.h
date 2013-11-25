#ifndef __NET_H__
#define __NET_H__

#include "base.h"
#include "callback.h"

namespace native
{
    namespace net
    {
        typedef sockaddr_in ip4_addr;
        typedef sockaddr_in6 ip6_addr;

        ip4_addr to_ip4_addr(const std::string& ip, int port) { return uv_ip4_addr(ip.c_str(), port); }
        ip6_addr to_ip6_addr(const std::string& ip, int port) { return uv_ip6_addr(ip.c_str(), port); }

        bool from_ip4_addr(ip4_addr* src, std::string& ip, int& port)
        {
            char dest[16];
            if(uv_ip4_name(src, dest, 16) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin_port));
                return true;
            }
            return false;
        }

        bool from_ip6_addr(ip6_addr* src, std::string& ip, int& port)
        {
            char dest[46];
            if(uv_ip6_name(src, dest, 46) == 0)
            {
                ip = dest;
                port = static_cast<int>(ntohs(src->sin6_port));
                return true;
            }
            return false;
        }
    }
}

#endif
