#ifndef UV_11_H
#define UV_11_H

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <utility>
#include <uv.h>
#include <http_parser.h>

namespace uv 
{
    class loop;
    class handle;
    class err;
    class handle;
    class stream;
    class tcp;
    class udp;
    class pipe;
    class req;
    class shutdown;
    class write;
    class connect;
    class timer;
    class async;
    class buf;

    typedef std::shared_ptr<loop> loop_ptr;
    typedef std::shared_ptr<handle> handle_ptr;
    typedef std::shared_ptr<stream> stream_ptr;
    typedef std::shared_ptr<tcp> tcp_ptr;

    void delete_handle(uv_handle_t* h);

    typedef uv_handle_type handle_type;
    typedef uv_req_type req_type;
    typedef uv_membership membership;

    typedef uv_alloc_cb alloc_cb;    
    typedef uv_read_cb read_cb;
    typedef uv_read2_cb read2_cb;
    typedef uv_write_cb write_cb;
    typedef uv_connect_cb connect_cb;
    typedef uv_shutdown_cb shutdown_cb;
    typedef uv_shutdown_cb shutdown_cb;
    typedef uv_connection_cb connection_cb;
    typedef uv_close_cb close_cb;
    typedef uv_timer_cb timer_cb;
    typedef uv_async_cb async_cb;
    typedef uv_shutdown_cb shutdown_cb;
    typedef uv_prepare_cb prepare_cb;
    typedef uv_check_cb check_cb;
    typedef uv_idle_cb idle_cb;
    typedef uv_getaddrinfo_cb getaddrinfo_cb;
    typedef uv_exit_cb exit_cb;
    typedef uv_fs_cb fs_cb;
    typedef uv_work_cb work_cb;
    typedef uv_after_work_cb after_work_cb;
    typedef uv_fs_event_cb fs_event_cb;

    class exception 
    {
    public:
        exception(const std::string& message)
            : message_(message)
        {}

        const std::string& message() const { return message_; }
        
    private:
        std::string message_;                
    };

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

    // SCO: serialized callback object
    template<typename callback_t, typename data_t=void>
    class sco
    {
    private:
        sco(const callback_t& callback, data_t* data)
            : callback_(callback), data_(data)
        { }

    public:
        ~sco() = default;

    public:       
        // TODO: after invoke(), sco object is deleted. this is not good!
        template<typename T, typename ...A>
        static void invoke(T* target, A&& ... args)
        {
        	std::shared_ptr<sco>(reinterpret_cast<sco*>(target->data))->callback_(std::forward<A>(args)...);
        }

        template<typename T>
		static data_t* get_data(T* target)
		{
			return reinterpret_cast<sco*>(target->data)->data_;
		}

        template<typename T>
        static void store(T* target, const callback_t& cb, data_t* data=nullptr)
        {
            target->data = new sco(cb, data);
        }

    private:
        callback_t callback_;
        data_t* data_;
    };

    class err 
    {
    public:
        err(uv_err_t e) : uv_err_(e) {}
        ~err() = default;

    public:
        const char* name() const { return uv_err_name(uv_err_); }
        const char* str() const { return uv_strerror(uv_err_); }

    private:
        uv_err_t uv_err_;
    };

	// uv_loop_t wrapper
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

    	int run() { return uv_run(uv_loop_); }
        int run_once() { return uv_run_once(uv_loop_); }
        static int run_default() { return uv_run(uv_default_loop()); }
        static int run_default_once() { return uv_run_once(uv_default_loop()); }

        void ref() { uv_ref(uv_loop_); }
        void unref() { uv_unref(uv_loop_); }
        void update_time() { uv_update_time(uv_loop_); }
        int64_t now() { return uv_now(uv_loop_); }

        err last_error() { return uv_last_error(uv_loop_); }
        static err last_error_default() { return uv_last_error(uv_default_loop()); }
        
    private:
        loop(const loop&);
        void operator =(const loop&);

    private:
    	uv_loop_t* uv_loop_;
    };

    class req
    {
    protected:
        req() {};
        virtual ~req() {}

    public:
        virtual uv_req_t* get() = 0;

        req_type type() { return get()->type; }
        void* data() { return get()->data; }
    };

    class handle
    {
    public:
        template<typename T>        
        handle(T* x)
            : uv_handle_(reinterpret_cast<uv_handle_t*>(x))
        { 
        	//printf("handle(): %x\n", this);
            assert(x);
        }

        virtual ~handle()
        {
        	//printf("~handle(): %x\n", this);
            uv_handle_ = nullptr;
        }

        handle(const handle& h)
            : uv_handle_(h.uv_handle_)
        { 
        	//printf("handle(const handle&): %x\n", this);
        }

    public:        
        template<typename T=uv_handle_t>
        T* get() { return reinterpret_cast<T*>(uv_handle_); }

        template<typename T=uv_handle_t>
        const T* get() const { return reinterpret_cast<T*>(uv_handle_); }

        bool is_active() { return uv_is_active(get()) != 0; }
        
        template<typename F>
        void close(F callback)
        {
			sco<F>::store(get(), callback);
			uv_close(get(),
				[](uv_handle_t* h) {
					sco<F>::invoke(h);
					delete_handle(h);
				});
        }

        handle& operator =(const handle& h)
        {
            uv_handle_ = h.uv_handle_;
            return *this;
        }

    protected:
        uv_handle_t* uv_handle_;
    };

    class stream : public handle 
    {
    public:
        template<typename T>
        stream(T* x)
            : handle(x)
        { }

        int shutdown(shutdown_cb cb) 
        {
            auto req = new uv_shutdown_t;
            req->data = this;
            return uv_shutdown(req, get<uv_stream_t>(), cb);
        }

        template<typename F>
        bool listen(F callback, int backlog=128)
        { 
            sco<F>::store(get(), callback);
            return uv_listen(get<uv_stream_t>(), backlog, 
                [](uv_stream_t* s, int status) {
                    sco<F>::invoke(s, status);
                }) == 0;
        }
        
        bool accept(stream_ptr client)
        { 
            return uv_accept(get<uv_stream_t>(), client->get<uv_stream_t>()) == 0;
        }

        template<typename F>
        bool read_start(F callback)
        {
            sco<F>::store(get(), callback);

            return uv_read_start(get<uv_stream_t>(),             
                [](uv_handle_t*, size_t suggested_size){
                    return uv_buf_t { new char[suggested_size], suggested_size };
                }, 
                [](uv_stream_t* s, ssize_t nread, uv_buf_t buf){
                    if(nread < 0)
                    {
                        assert(uv_last_error(s->loop).code == UV_EOF);
                        sco<F>::invoke(s,
                            nullptr, 
                            static_cast<int>(nread));
                    } 
                    else if(nread >= 0)
                    {
                        sco<F>::invoke(s,
                            buf.base, 
                            static_cast<int>(nread));
                    }
                    delete buf.base;                                      
                }) == 0;      
        }

        bool read_stop()
        {
        	return uv_read_stop(get<uv_stream_t>()) == 0;
        }

        // TODO: implement read2_start()
        //int read2_start(alloc_cb a, read2_cb r) { return uv_read2_start(get<uv_stream_t>(), a, r); }                

        bool write(write_cb cb) 
        {
            /*
            uv_buf_t bufs[] = {
                { "Hello, World", 11 },
                { "Hello, World", 11 },
            };

            uv_write_t* w = new uv_write_t;
            return uv_write(w, get<uv_stream_t>(), bufs, 2, cb);
            */
            return false;
        }

        int write2(buf& b, stream& send_handle, write_cb cb);
        int write(std::vector<buf>& bufs, write_cb cb);
        int write2(std::vector<buf>& bufs, stream& send_handle, write_cb cb);
    };

    class tcp : public stream
    {        
    public:
        template<typename X>
        tcp(X* x)
            : stream(x)
        { }

    public:
        tcp()
            : stream(new uv_tcp_t)
        {
            uv_tcp_init(uv_default_loop(), get<uv_tcp_t>());            
        }

        tcp(loop& l)
            : stream(new uv_tcp_t)
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

    void delete_handle(uv_handle_t* h)
    {
        switch(h->type)
        {
            case UV_TCP: delete reinterpret_cast<uv_tcp_t*>(h); break;                            
            default: assert(0); break;
        }
    }    
}

namespace http 
{
    typedef http_method method;
    typedef http_parser_url_fields url_fields;
    typedef http_errno error;

    const char* get_error_name(error err) 
    {
        return http_errno_name(err);
    }

    const char* get_error_description(error err) 
    {
        return http_errno_description(err);
    }

    const char* get_method_name(method m) 
    {
        return http_method_str(m);
    }

    class url_parse_exception { };

    class url 
    {
    public:
        url(const char* data, std::size_t len, bool is_connect=false)
            : handle_(), buf_(data, len) 
        { 
            assert(data && len);

            if(http_parser_parse_url(data, len, is_connect, &handle_) != 0)
            {
                // failed for some reason
                // TODO: let the caller know the error code (or error message)
                throw url_parse_exception();
            }
        }

        url() = default;
        url(const url&) = default;
        url& operator =(const url&) = default;
        ~url() = default;

    public:
        bool has_schema() const 
        {
            return handle_.field_set & (1<<UF_SCHEMA);
        }

        std::string schema() const 
        {
            if(has_schema()) 
            {
                return buf_.substr(handle_.field_data[UF_SCHEMA].off, handle_.field_data[UF_SCHEMA].len);
            }

            return std::string();
        }

        bool has_host() const 
        {
            return handle_.field_set & (1<<UF_HOST);
        }

        std::string host() const 
        {
            if(has_host()) 
            {
                return buf_.substr(handle_.field_data[UF_HOST].off, handle_.field_data[UF_HOST].len);
            }

            return std::string();
        }

        bool has_port() const 
        {
            return handle_.field_set & (1<<UF_PORT);
        }

        int port() const 
        {
            return static_cast<int>(handle_.port);
        }

        bool has_path() const 
        {
            return handle_.field_set & (1<<UF_PATH);
        }

        std::string path() const 
        {
            if(has_path())
            {
                return buf_.substr(handle_.field_data[UF_PATH].off, handle_.field_data[UF_PATH].len);
            }

            return std::string();
        }

        bool has_query() const 
        {
            return handle_.field_set & (1<<UF_QUERY);
        }

        std::string query() const 
        {
            if(has_query()) 
            {
                return buf_.substr(handle_.field_data[UF_QUERY].off, handle_.field_data[UF_QUERY].len);
            }

            return std::string();
        }

        bool has_fragment() const 
        {
            return handle_.field_set & (1<<UF_FRAGMENT);
        }

        std::string fragment() const 
        {
            if(has_fragment()) 
            {
                return buf_.substr(handle_.field_data[UF_FRAGMENT].off, handle_.field_data[UF_FRAGMENT].len);
            }

            return std::string();
        }

    private:
        http_parser_url handle_;
        std::string buf_;
    };

    class parser 
    {
        typedef http_parser_type parser_type;

    public:
        parser(parser_type type)
            : handle_(), schema_(), host_(), port_(), path_(), query_(), fragment_(), headers_(), body_()
        {
            http_parser_init(&handle_, type);
        }

        virtual ~parser() {
        }

    protected:
        template<typename cb_t>
        void set_message_begin_callback(cb_t callback)
        {            
        }

        template<typename cb_t>
        void set_url_callback(cb_t callback)
        {            
        }
    
        std::size_t parse(const char* data, std::size_t len)
        {
            handle_.data = this;

            http_parser_settings settings;
            settings.on_url = [](http_parser* h, const char *at, size_t length) {
                auto p = reinterpret_cast<parser*>(h->data);
                assert(p);

                url u(at, length);

                p->schema_ = u.has_schema() ? u.schema() : "HTTP";
                p->host_ = u.has_host() ? u.host() : "";
                p->port_ = u.has_port() ? u.port() : (p->schema_ == "HTTP" ? 80 : 443);
                p->path_ = u.has_path() ? u.path() : "/";
                p->query_ = u.has_query() ? u.query() : "";
                p->fragment_ = u.has_fragment() ? u.fragment() : "";                

                return 0;
            };

            return http_parser_execute(&handle_, &settings, data, len);
        }

    private:
        // copy prevention
        parser(const parser&);
        void operator =(const parser&);

    private:
        http_parser handle_;        
        std::string schema_;
        std::string host_;
        int port_;
        std::string path_;
        std::string query_;
        std::string fragment_;
        std::map<std::string, std::string> headers_;
        std::string body_;
    };

    class request : public parser 
    {
    public:
        request()
            : parser(HTTP_REQUEST)
        {
        }

        virtual ~request() 
        {
        }
    };

    class response : public parser 
    {
    public:
        response()
            : parser(HTTP_RESPONSE)
        {
        }

        virtual ~response() 
        {
        }
    };
}

#endif
