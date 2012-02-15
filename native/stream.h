#ifndef __STREAM_H__
#define __STREAM_H__

#include "base.h"
#include "error.h"
#include "handle.h"
#include "callback.h"
#include "events.h"

namespace native
{
	namespace detail
	{
		class stream : public handle
		{
		public:
			template<typename T>
			stream(T* x)
				: handle(x)
			{ }

			bool listen(std::function<void(Error)> callback, int backlog=128)
			{
				callbacks::store(get()->data, uv_cid_listen, callback);
				return uv_listen(get<uv_stream_t>(), backlog, [](uv_stream_t* s, int status) {
                    callbacks::invoke<decltype(callback)>(s->data, uv_cid_listen, Error::fromStatusCode(status));
                }) == 0;
			}

			bool accept(stream* client)
			{
				return uv_accept(get<uv_stream_t>(), client->get<uv_stream_t>()) == 0;
			}

			bool read_start(std::function<void(const char* buf, ssize_t len)> callback)
			{
				return read_start<0>(callback);
			}

			template<int max_alloc_size>
			bool read_start(std::function<void(const char* buf, ssize_t len)> callback)
			{
				callbacks::store(get()->data, uv_cid_read_start, callback);

				return uv_read_start(get<uv_stream_t>(),
					[](uv_handle_t*, size_t suggested_size){
						if(!max_alloc_size)
						{
							return uv_buf_t { new char[suggested_size], suggested_size };
						}
						else
						{
							auto size = max_alloc_size > suggested_size ? suggested_size : max_alloc_size;
							return uv_buf_t { new char[size], size };
						}
					},
					[](uv_stream_t* s, ssize_t nread, uv_buf_t buf){
					    callbacks::invoke<decltype(callback)>(s->data, uv_cid_read_start, buf.base, nread);
						delete buf.base;
					}) == 0;
			}

			bool read_stop()
			{
				return uv_read_stop(get<uv_stream_t>()) == 0;
			}

			// TODO: implement read2_start()

			bool write(const char* buf, int len, std::function<void(Error)> callback)
			{
				uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(buf), len } };
				callbacks::store(get()->data, uv_cid_write, callback);
				return uv_write(new uv_write_t, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
					callbacks::invoke<decltype(callback)>(req->handle->data, uv_cid_write, Error::fromStatusCode(status));
					delete req;
				}) == 0;
			}

			bool write(const std::string& buf, std::function<void(Error)> callback)
            {
                uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(buf.c_str()), buf.length()} };
                callbacks::store(get()->data, uv_cid_write, callback);
                return uv_write(new uv_write_t, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
                    callbacks::invoke<decltype(callback)>(req->handle->data, uv_cid_write, Error::fromStatusCode(status));
                    delete req;
                }) == 0;
            }

            bool write(const std::vector<char>& buf, std::function<void(Error)> callback)
            {
                uv_buf_t bufs[] = { uv_buf_t { const_cast<char*>(&buf[0]), buf.size() } };
                callbacks::store(get()->data, uv_cid_write, callback);
                return uv_write(new uv_write_t, get<uv_stream_t>(), bufs, 1, [](uv_write_t* req, int status) {
                    callbacks::invoke<decltype(callback)>(req->handle->data, uv_cid_write, Error::fromStatusCode(status));
                    delete req;
                }) == 0;
            }

            // TODO: implement write2()

			bool shutdown(std::function<void(Error)> callback)
			{
				callbacks::store(get()->data, uv_cid_shutdown, callback);
				return uv_shutdown(new uv_shutdown_t, get<uv_stream_t>(), [](uv_shutdown_t* req, int status) {
					callbacks::invoke<decltype(callback)>(req->handle->data, uv_cid_shutdown, Error::fromStatusCode(status));
					delete req;
				}) == 0;
			}
		};
	}
#if 0
	template<bool R, bool W, typename ...E>
	class StreamT;

	typedef StreamT<true, false> ReadableStream;
	typedef StreamT<false, true> WritableStream;
	typedef StreamT<true, true> Stream;

	typedef std::tuple<
	    ev::data, std::function<void(const std::string&)>,
	    ev::end, std::function<void()>,
	    ev::error, std::function<void(Error)>,
	    ev::close, std::function<void()>,
	    ev::drain, std::function<void()>,
	    ev::pipe, std::function<void(const Stream&)>
	> stream_events;

	template<bool R, bool W, typename ...E>
    class StreamT : public EventEmitter<stream_events, E...>
    {
    protected:
        StreamT(uv_stream_t* handle)
            : stream_(handle)
            , readable_(R)
            , writable_(W)
        {
            // TODO: handle memory failure
            assert(handle);

            if(R)
            {
                startReading();
            }
        }

    public:
        virtual ~StreamT()
        {
            destroy();
        }

    public:
        bool readable() const { return readable_; }
        bool writable() const { return writable_; }

        void setEncoding(const std::string& encoding)
        {
        }

        void pause()
        {
            static_assert(R, "StreamT::pause() is only available for readable stream types.");
            get()->read_stop();
        }
        void resume()
        {
            static_assert(R, "StreamT::resume() is only available for readable stream types.");
            startReading();
        }

        void destroy()
        {
            if(stream_)
            {
                // TODO: is it necessary to stop reading before closing handle?
                get()->read_stop();

                stream_->close([&]() {
                    readable_ = false;
                    writable_ = false;
                });
                stream_.reset();

                this->template emit<ev::close>();
            }
        }

        void destroySoon()
        {
            // TODO: implement StreamT::destroySoon()
        }

        bool pipe(StreamT& destination, bool end=true) { return false; }

        // TODO: optional argument: 'fd' in Unix
        bool write(const std::string& str, const std::string& encoding, std::function<void()> callback)
        {
            static_assert(W, "StreamT::write() is only available for writable stream types.");

            return write_(str, encoding);
        }

        bool end() { return false; }
        bool end(const std::wstring& str, const std::string& encoding) { return false; }

    protected:
        detail::stream* get() { return stream_.get(); }

        void startReading()
        {
            stream_->read_start([&](const char* buf, int len) {
                if(len < 0)
                {
                    readable_ = false;

                    Error e = Error::getLastError();
                    if(e == UV_EOF)
                    {
                        this->template emit<ev::end>();
                    }
                    else
                    {
                        this->template emit<ev::error>(e);
                    }
                }
                else
                {
                    this->template emit<ev::data>(std::string(buf, len));
                }
            });
        }

    private:
        virtual bool write_(const std::string& str, const std::string& encoding, std::function<void()> callback) = 0;

    private:
        // TODO: do we still need this pointer?
        std::shared_ptr<detail::stream> stream_;

        bool readable_;
        bool writable_;
    };
#endif
}

#endif
