#ifndef __NET_H__
#define __NET_H__

#include "base.h"
#include "detail.h"
#include "stream.h"
#include "error.h"
#include "process.h"
#include "timers.h"
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

        struct SocketType
        {
            enum
            {
                IPv4,
                IPv6,
                Pipe,
                None
            };
        };

        class Server;

        class Socket : public Stream
        {
            friend class Server;

            static constexpr int FLAG_GOT_EOF = 1 << 0;
            static constexpr int FLAG_SHUTDOWN = 1 << 1;
            static constexpr int FLAG_DESTROY_SOON = 1 << 2;
            static constexpr int FLAG_SHUTDOWNQUED = 1 << 3;

        protected:
            Socket(detail::stream* handle, Server* server, bool allowHalfOpen)
                : Stream(handle, true, true)
                , stream_(handle)
                , server_(server)
                , flags_(0)
                , allow_half_open_(allowHalfOpen)
                , connecting_(false)
                , destroyed_(false)
                , bytes_written_(0)
                , bytes_read_(0)
                , pending_write_reqs_()
                , connect_queue_size_(0)
                , connect_queue_()
                , on_data_callback_()
                , on_end_callback_()
                , on_destroy_callback_()
            {
                registerEvent<ev::connect>();
                registerEvent<ev::data>();
                registerEvent<ev::end>();
                registerEvent<ev::timeout>();
                registerEvent<ev::drain>();
                registerEvent<ev::error>();
                registerEvent<ev::close>();

                if(stream_)
                {
                    stream_->set_on_read_callback([&](const char* buffer, std::size_t offset, std::size_t length, detail::error e){
                        on_read(buffer, offset, length, e);
                    });
                }
            }

            virtual ~Socket()
            {
            }

        public:
            virtual bool end(const Buffer& buffer)
            {
                if(connecting_ && ((flags_ & FLAG_SHUTDOWNQUED) == 0))
                {
                    if(buffer.size()) write(buffer);
                    writable(false);
                    flags_ |= FLAG_SHUTDOWNQUED;
                }

                if(!writable()) return false;
                writable(false);

                if(buffer.size()) write(buffer);

                if(!readable())
                {
                    destroySoon();
                }
                else
                {
                    flags_ |= FLAG_SHUTDOWN;
                    detail::error e = stream_->shutdown([&](detail::error e){
                        assert(flags_ & FLAG_SHUTDOWN);
                        assert(!writable());

                        if(destroyed_) return;

                        if((flags_ & FLAG_GOT_EOF) || !readable())
                        {
                            destroy();
                        }
                    });
                    if(e)
                    {
                        destroy(e);
                        return false;
                    }
                }

                return true;
            }

            virtual bool end(const std::string& str, const std::string& encoding=std::string(), int fd=-1)
            {
                // TODO: what about 'fd'?
                return end(Buffer(str, encoding));
            }
            virtual bool end()
            {
                return end(Buffer(nullptr));
            }

            // TODO: these overloads are inherited but not used for Socket.
            //virtual bool write(const Buffer& buffer) { assert(false); return false; } //-> virtual bool write(const Buffer& buffer, write_callback_type callback=nullptr)
            virtual bool write(const std::string& str, const std::string& encoding, int fd) { assert(false); return false; }

            // TODO: this is not inherited from Stream - a new overload.
            // callback is invoked after all data is written.
            virtual bool write(const Buffer& buffer, std::function<void()> callback=nullptr)
            {
                bytes_written_ += buffer.size();

                if(connecting_)
                {
                    connect_queue_size_ += buffer.size();
                    connect_queue_.push_back(std::make_pair(std::shared_ptr<Buffer>(new Buffer(buffer)), callback));
                    return false;
                }

                timers::active(this);

                if(!stream_) throw Exception("The socket is closed.");

                detail::error e = stream_->write(buffer.base(), 0, buffer.size(), [=](detail::error err){
                    if(destroyed_) return;

                    if(err)
                    {
                        destroy(err);
                        return;
                    }

                    timers::active(this);

                    pending_write_reqs_--;
                    if(pending_write_reqs_ == 0) emit<ev::drain>();

                    if(callback) callback();

                    if((pending_write_reqs_== 0) && (flags_ & FLAG_DESTROY_SOON))
                    {
                        destroy();
                    }
                });
                if(e)
                {
                    destroy(e);
                    return false;
                }

                pending_write_reqs_++;

                return stream_->write_queue_size() == 0;
            }

            virtual void destroy(Exception exception)
            {
                destroy_(true, exception);
            }

            virtual void destroy()
            {
                destroy_(false, Exception());
            }

            virtual void pause()
            {
                if(stream_)
                {
                    stream_->read_stop();
                    stream_->unref(); // See tcp::unref() does nothing.
                }
            }

            virtual void resume()
            {
                if(stream_)
                {
                    stream_->read_start();
                    stream_->ref(); // See tcp::unref() does nothing.
                }
            }

            virtual void destroySoon()
            {
                writable(false);
                flags_ |= FLAG_DESTROY_SOON;

                if(pending_write_reqs_ == 0)
                {
                    destroy();
                }
            }

        private:
            virtual void destroy_(bool failed, Exception exception)
            {
                if(destroyed_) return;

                connect_queue_cleanup();

                readable(false);
                writable(false);

                timers::unenroll(this);

                // Because of C++ declration order, we cannot call Server class member functions here.
                // Instead, Server handles this using delegate.
#if 1
                if(on_destroy_callback_) on_destroy_callback_();
#else
                if(server_)
                {
                    //server_->connections_--;
                    //server_->emitCloseIfDrained();
                }
#endif

                if(stream_)
                {
                    stream_->close();
                    stream_->set_on_read_callback(nullptr);
                    stream_ = nullptr;
                }

                process::nextTick([&](){
                    if(failed) emit<ev::error>(exception);

                    // TODO: node.js implementation pass one argument whether there was errors or not.
                    //emit<ev::close>(failed);
                    emit<ev::close>();
                });
            }

            void connect_queue_cleanup()
            {
                connecting_ = false;
                connect_queue_size_ = 0;
                connect_queue_.clear();
            }

            void on_read(const char* buffer, std::size_t offset, std::size_t length, detail::error e)
            {
                timers::active(this);

                std::size_t end_pos = offset + length;
                if(buffer)
                {
                    // TODO: implement decoding
                    // ..
                    if(haveListener<ev::data>()) emit<ev::data>(Buffer(&buffer[offset], length));

                    bytes_read_ += length;

                    if(on_data_callback_) on_data_callback_(buffer, offset, end_pos);
                }
                else
                {
                    if(e.code() == UV_EOF)
                    {
                        readable(false);

                        assert(!(flags_ & FLAG_GOT_EOF));
                        flags_ |= FLAG_GOT_EOF;

                        if(!writable()) destroy();

                        if(!allow_half_open_) end();
                        if(haveListener<ev::end>()) emit<ev::end>();
                        if(on_end_callback_) on_end_callback_();
                    }
                    else if(e.code() == ECONNRESET)
                    {
                        destroy();
                    }
                    else
                    {
                        destroy(Exception(e));
                    }
                }
            }

        private:
            detail::stream* stream_;

            int flags_;
            bool allow_half_open_;
            bool connecting_;
            bool destroyed_;

            Server* server_;

            std::size_t bytes_written_;
            std::size_t bytes_read_;
            std::size_t pending_write_reqs_;

            std::size_t connect_queue_size_;
            std::vector<std::pair<std::shared_ptr<Buffer>, std::function<void()>>> connect_queue_;

            // ondata: (buffer, offset, end)
            std::function<void(const char*, std::size_t, std::size_t)> on_data_callback_;

            std::function<void()> on_end_callback_;

            // TODO: only Server class must use this delegate.
            std::function<void()> on_destroy_callback_;
        };

        class Server : public EventEmitter
        {
            friend Server* createServer(std::function<void(Server*)>, bool);

            Server(bool allowHalfOpen)
                : EventEmitter()
                , stream_(nullptr)
                , connections_(0)
                , max_connections_(0)
                , allow_half_open_(allowHalfOpen)
                , backlog_(128)
                , socket_type_(SocketType::None)
                , pipe_name_()
            {
                registerEvent<ev::listening>();
                registerEvent<ev::connection>();
                registerEvent<ev::close>();
                registerEvent<ev::error>();
            }

            virtual ~Server()
            {}

        public:
            // listen over TCP socket
            bool listen(int port, const std::string& host=std::string("0.0.0.0"), ev::listening::callback_type listeningListener=nullptr)
            {
                return listen_(host, port, listeningListener);
            }

            // listen over unix-socket
            bool listen(const std::string& path, ev::listening::callback_type listeningListener=nullptr)
            {
                return listen_(path, 0, listeningListener);
            }

            // TODO: implement Socket::pause() function.
            void pause(unsigned int msecs) {}

            void close()
            {
                if(!stream_)
                {
                    throw Exception("Server is not running (1).");
                }

                stream_->close();
                stream_ = nullptr;
                emitCloseIfDrained();
            }

            int address(std::string& ip_or_pipe_name, int& port)
            {
                if(!stream_) return SocketType::None;

                switch(socket_type_)
                {
                case SocketType::IPv4:
                    {
                        auto x = dynamic_cast<detail::tcp*>(stream_);
                        assert(x);

                        bool b;
                        x->get_sock_name(b, ip_or_pipe_name, port);
                        assert(b == true);
                    }
                    break;

                case SocketType::IPv6:
                    {
                        auto x = dynamic_cast<detail::tcp*>(stream_);
                        assert(x);

                        bool b;
                        x->get_sock_name(b, ip_or_pipe_name, port);
                        assert(b == false);
                    }
                    break;

                case SocketType::Pipe:
                    {
                        ip_or_pipe_name = pipe_name_;
                    }
                    break;
                }

                return socket_type_;
            }

            std::size_t maxConnections() const { return max_connections_; }
            void maxConnections(std::size_t value) { max_connections_ = value; }

            std::size_t connections() const { return connections_; }

        protected:
            void emitCloseIfDrained()
            {
                if(stream_ || connections_) return;

                process::nextTick([&](){ emit<ev::close>(); });
            }

            detail::stream* create_server_handle(const std::string& ip_or_pipe_name, int port)
            {
                detail::error e;
                if(isIPv4(ip_or_pipe_name))
                {
                    auto handle = new detail::tcp();
                    assert(handle);

                    auto err = handle->bind(ip_or_pipe_name, port);
                    if(err)
                    {
                        handle->close();
                        return nullptr;
                    }

                    socket_type_ = SocketType::IPv4;
                    return handle;
                }
                else if(isIPv6(ip_or_pipe_name))
                {
                    auto handle = new detail::tcp();
                    assert(handle);

                    auto err = handle->bind6(ip_or_pipe_name, port);
                    if(err)
                    {
                        handle->close();
                        return nullptr;
                    }

                    socket_type_ = SocketType::IPv6;
                    return handle;
                }
                else
                {
                    auto handle = new detail::pipe();
                    assert(handle);

                    auto err = handle->bind(ip_or_pipe_name);
                    if(err)
                    {
                        handle->close();
                        return nullptr;
                    }

                    socket_type_ = SocketType::Pipe;
                    pipe_name_ = ip_or_pipe_name;
                    return handle;
                }
            }

            bool listen_(const std::string& ip_or_pipe_name, int port, ev::listening::callback_type listeningListener)
            {
                if(listeningListener) on<ev::listening>(listeningListener);

                if(!stream_)
                {
                    stream_ = create_server_handle(ip_or_pipe_name, port);
                    if(!stream_)
                    {
                        process::nextTick([&](){
                            emit<ev::error>(Exception("Failed to create a server socket (1)."));
                        });
                        return false;
                    }
                }

                detail::error e = stream_->listen(backlog_, [&](detail::stream* s, detail::error e){
                    if(e)
                    {
                        emit<ev::error>(Exception(e, "Failed to accept client socket (1)."));
                    }
                    else
                    {
                        if(max_connections_ && connections_ > max_connections_)
                        {
                            // TODO: where is "error" event?
                            s->close(); // just close down?
                            return;
                        }

                        auto socket = new Socket(s, this, allow_half_open_);
                        assert(socket);

                        socket->on_destroy_callback_ = [&](){
                            connections_--;
                            emitCloseIfDrained();
                        };

                        socket->resume();

                        connections_++;
                        emit<ev::connection>(socket);

                        socket->emit<ev::connect>();
                    }
                });

                if(e)
                {
                    stream_->close();
                    stream_ = nullptr;
                    process::nextTick([&](){
                        emit<ev::error>(Exception(e, "Failed to initiate listening on server socket (1)."));
                    });
                    return false;
                }

                process::nextTick([&](){
                    emit<ev::listening>();
                });
                return true;
            }

        public:
            detail::stream* stream_;
            std::size_t connections_;
            std::size_t max_connections_;
            bool allow_half_open_;
            int backlog_;
            int socket_type_;
            std::string pipe_name_;
        };

        Server* createServer(std::function<void(Server*)> callback, bool allowHalfOpen=false)
        {
            auto x = new Server(allowHalfOpen);
            assert(x);

            process::nextTick([=](){
                callback(x);
            });

            return x;
        }
    }
}

#endif
