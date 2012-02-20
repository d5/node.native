#ifndef __STREAM_H__
#define __STREAM_H__

#include "base.h"
#include "detail.h"
#include "error.h"
#include "buffers.h"
#include "events.h"

namespace native
{
    class Stream : public EventEmitter
    {
    public:
        Stream(detail::stream* stream, bool readable=true, bool writable=true)
            : EventEmitter()
            , stream_(stream)
            , readable_(readable)
            , writable_(writable)
        {
            assert(stream_);

            if(readable_) registerEvent<ev::data>();
            if(readable_) registerEvent<ev::end>();
            registerEvent<ev::error>();
            registerEvent<ev::close>();
            if(writable_) registerEvent<ev::drain>();
            if(writable_) registerEvent<ev::pipe>();
        }

        virtual ~Stream()
        {}

    public:
        bool readable() const { return readable_; }
        bool writable() const { return writable_; }

        // TODO: implement Stream::setEncoding() function.
        void setEncoding(const std::string& encoding)
        {}

        virtual void pause() = 0;
        virtual void resume() = 0;
        virtual void destroy(Exception exception) = 0;
        virtual void destroy() = 0;
        virtual void destroySoon() = 0;

        virtual bool pipe(Stream& destination, const util::dict& options)
        {
            // TODO: implement Stream::pipe() function.
#if 0
            auto source = *this;

            // whenever data is available from source, copy it destination
            auto src_data_l = source.on<ev::data>([&](const std::vector<char>& chunk){
                if(destination.writable())
                {
                    if(!destination.write(chunk))
                    {
                        // destination is full: pause source
                        source.pause();
                    }
                }
            });

            // when the destination gets writable again, resume reading
            destination.on<ev::drain>([&](){
                if(source.readable()) source.resume();
            });

            //
            auto cleanUp = [&](){
                // !!!!!!!!
            };

            // "end" event callback
            bool didOnEnd = false;
            auto onEnd = [&](){
                if(didOnEnd) return;
                didOnEnd = true;

                cleanUp();
                destination.end();
            };

            auto onClose = [&](){
                // !!!!!!!!
            };

            // assign "end", "close" event callback
            if(!destination.props().has("_isStdio") && !options.compare("end", "false"))
            {
                source.on<ev::end>(onEnd);
                source.on<ev::close>(onClose);
            }

            // !!!!!!!!
#endif
            return false;
        }

        virtual bool write(const Buffer& buffer, std::function<void()> callback=nullptr) = 0;
        virtual bool write(const std::string& str, const std::string& encoding, int fd) = 0;

        virtual bool end(const Buffer& buffer) = 0;
        virtual bool end(const std::string& str, const std::string& encoding, int fd) = 0;
        virtual bool end() = 0;

    protected:
        void writable(bool b) { writable_ = b; }
        void readable(bool b) { readable_ = b; }

    private:
        detail::stream* stream_;
        bool readable_;
        bool writable_;
    };
}

#endif
