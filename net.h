#ifndef __NET_H__
#define __NET_H__

#include <string>
#include <uv.h>
#include "base.h"
#include "handle.h"
#include "stream.h"

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

		class tcp : public native::base::stream
		{
		public:
			template<typename X>
			tcp(X* x)
				: stream(x)
			{ }

		public:
			tcp()
				: native::base::stream(new uv_tcp_t)
			{
				uv_tcp_init(uv_default_loop(), get<uv_tcp_t>());
			}

			tcp(native::base::loop& l)
				: native::base::stream(new uv_tcp_t)
			{
				uv_tcp_init(l.get(), get<uv_tcp_t>());
			}

			bool nodelay(bool enable) { return uv_tcp_nodelay(get<uv_tcp_t>(), enable?1:0) == 0; }
			bool keepalive(bool enable, unsigned int delay) { return uv_tcp_keepalive(get<uv_tcp_t>(), enable?1:0, delay) == 0; }
			bool simultanious_accepts(bool enable) { return uv_tcp_simultaneous_accepts(get<uv_tcp_t>(), enable?1:0) == 0; }

			bool bind(const std::string& ip, int port) { return uv_tcp_bind(get<uv_tcp_t>(), uv_ip4_addr(ip.c_str(), port)) == 0; }
			bool bind6(const std::string& ip, int port) { return uv_tcp_bind6(get<uv_tcp_t>(), uv_ip6_addr(ip.c_str(), port)) == 0; }

			bool getsockname(bool& ip4, std::string& ip, int& port)
			{
				struct sockaddr_storage addr;
				int len = sizeof(addr);
				if(uv_tcp_getsockname(get<uv_tcp_t>(), reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
				{
					ip4 = (addr.ss_family == AF_INET);
					if(ip4) return from_ip4_addr(reinterpret_cast<ip4_addr*>(&addr), ip, port);
					else return from_ip6_addr(reinterpret_cast<ip6_addr*>(&addr), ip, port);
				}
				return false;
			}

			bool getpeername(bool& ip4, std::string& ip, int& port)
			{
				struct sockaddr_storage addr;
				int len = sizeof(addr);
				if(uv_tcp_getpeername(get<uv_tcp_t>(), reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
				{
					ip4 = (addr.ss_family == AF_INET);
					if(ip4) return from_ip4_addr(reinterpret_cast<ip4_addr*>(&addr), ip, port);
					else return from_ip6_addr(reinterpret_cast<ip6_addr*>(&addr), ip, port);
				}
				return false;
			}
		};
	}
}

#endif
