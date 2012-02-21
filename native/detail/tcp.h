#ifndef __DETAIL_TCP_H__
#define __DETAIL_TCP_H__

#include "base.h"
#include "stream.h"

namespace native
{
    namespace detail
    {
        class tcp : public stream
        {
        public:
            tcp()
                : stream(reinterpret_cast<uv_stream_t*>(&tcp_))
                , tcp_()
            {
                int r = uv_tcp_init(uv_default_loop(), &tcp_);
                assert(r == 0);

                tcp_.data = this;
            }

        private:
            virtual ~tcp()
            {}

        public:
            // tcp hides handle::ref() and handle::unref()
            virtual void ref() {}
            virtual void unref() {}

            virtual std::shared_ptr<net_addr> get_sock_name()
            {
                struct sockaddr_storage addr;
                int addrlen = static_cast<int>(sizeof(addr));
                bool res = uv_tcp_getsockname(&tcp_, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) == 0;

                if(res)
                {
                    assert(addr.ss_family == AF_INET || addr.ss_family == AF_INET6);

                    auto na = std::shared_ptr<net_addr>(new net_addr);
                    na->is_ipv4 = (addr.ss_family == AF_INET);
                    if(na->is_ipv4)
                    {
                        if(from_ip4_addr(reinterpret_cast<struct sockaddr_in*>(&addr), na->ip, na->port)) return na;
                    }
                    else
                    {
                        if(from_ip6_addr(reinterpret_cast<struct sockaddr_in6*>(&addr), na->ip, na->port)) return na;
                    }
                }

                return nullptr;
            }

            virtual std::shared_ptr<net_addr> get_peer_name()
            {
                struct sockaddr_storage addr;
                int addrlen = static_cast<int>(sizeof(addr));
                bool res = uv_tcp_getsockname(&tcp_, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) == 0;

                if(res)
                {
                    assert(addr.ss_family == AF_INET || addr.ss_family == AF_INET6);

                    auto na = std::shared_ptr<net_addr>(new net_addr);
                    na->is_ipv4 = (addr.ss_family == AF_INET);
                    if(na->is_ipv4)
                    {
                        if(from_ip4_addr(reinterpret_cast<struct sockaddr_in*>(&addr), na->ip, na->port)) return na;
                    }
                    else
                    {
                        if(from_ip6_addr(reinterpret_cast<struct sockaddr_in6*>(&addr), na->ip, na->port)) return na;
                    }
                }

                return nullptr;
            }

            virtual resval set_no_delay(bool enable)
            {
                if(uv_tcp_nodelay(&tcp_, enable?1:0)) return get_last_error();
                return resval();
            }

            virtual resval set_keepalive(bool enable, unsigned int delay)
            {
                if(uv_tcp_keepalive(&tcp_, enable?1:0, delay)) return get_last_error();
                return resval();
            }

            virtual resval bind(const std::string& ip, int port)
            {
                struct sockaddr_in addr = to_ip4_addr(ip, port);

                if(uv_tcp_bind(&tcp_, addr)) return get_last_error();
                return resval();
            }

            virtual resval bind6(const std::string& ip, int port)
            {
                struct sockaddr_in6 addr = to_ip6_addr(ip, port);

                if(uv_tcp_bind6(&tcp_, addr)) return get_last_error();
                return resval();
            }

            virtual resval listen(int backlog, std::function<void(stream*, resval)> callback)
            {
                callbacks::store(lut(), cid_uv_listen, callback);
                bool res = uv_listen(reinterpret_cast<uv_stream_t*>(&tcp_), backlog, [](uv_stream_t* handle, int status) {
                    auto self = reinterpret_cast<tcp*>(handle->data);
                    assert(self);

                    if(status == 0)
                    {
                        auto client_obj = new tcp;
                        assert(client_obj);

                        int r = uv_accept(handle, reinterpret_cast<uv_stream_t*>(&client_obj->tcp_));
                        assert(r == 0);

                        callbacks::invoke<decltype(callback)>(self->lut(), cid_uv_listen, client_obj, resval());
                    }
                    else
                    {
                        callbacks::invoke<decltype(callback)>(self->lut(), cid_uv_listen, nullptr, get_last_error());
                    }
                }) == 0;

                return res?resval():get_last_error();
            }

            // TODO: Node.js implementation takes 5 parameter in callback function.
            virtual resval connect(const std::string& ip, int port, std::function<void(resval, bool, bool)> callback)
            {
                struct sockaddr_in addr = to_ip4_addr(ip, port);

                auto req = new uv_connect_t;
                assert(req);

                callbacks::store(lut(), cid_uv_connect, callback);

                bool res = uv_tcp_connect(req, &tcp_, addr, on_connect) == 0;
                if(!res) delete req;
                return res?resval():get_last_error();
            }

            // TODO: Node.js implementation takes 5 parameter in callback function.
            virtual resval connect6(const std::string& ip, int port, std::function<void(resval, bool, bool)> callback)
            {
                struct sockaddr_in6 addr = to_ip6_addr(ip, port);

                auto req = new uv_connect_t;
                assert(req);

                callbacks::store(lut(), cid_uv_connect6, callback);

                bool res = uv_tcp_connect6(req, &tcp_, addr, on_connect) == 0;
                if(!res) delete req;
                return res?resval():get_last_error();
            }

        protected:
            virtual stream* accept_new_()
            {
                auto x = new tcp;
                assert(x);

                int r = uv_accept(reinterpret_cast<uv_stream_t*>(&tcp_), reinterpret_cast<uv_stream_t*>(&x->tcp_));
                assert(r == 0);

                return x;
            }

        private:
            static void on_connect(uv_connect_t* req, int status)
            {
                auto self = reinterpret_cast<tcp*>(req->handle->data);
                assert(self);

                callbacks::invoke<std::function<void(resval, bool, bool)>>(self->lut(), cid_uv_connect,
                    status?get_last_error():resval(), true, true);

                delete req;
            }

        private:
            uv_tcp_t tcp_;
        };
    }
}

#endif
