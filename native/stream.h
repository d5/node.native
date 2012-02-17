#ifndef __STREAM_H__
#define __STREAM_H__

#include "base.h"
#include "detail.h"
#include "events.h"

namespace native
{
    class Stream;
    typedef std::shared_ptr<Stream> StreamPtr;

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

            if(readable_)
            {
                // TODO: start reading!
            }
        }

        virtual ~Stream()
        {}

    public:
        bool readable() const { return readable_; }
        bool writable() const { return writable_; }

        // TODO: implement Stream::setEncoding() function.
        void setEncoding(const std::string& encoding) { }

        virtual bool pause() { return false; }
        virtual bool resume() { return false; }
        virtual bool destroy() { return false; }
        virtual bool destroySoon() { return false; }

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

        virtual bool write(const std::vector<char>& buffer)
        {
            return false;
        }

        virtual bool end()
        {
            return false;
        }

        virtual bool end(const std::vector<char>& buffer)
        {
            return false;
        }

    private:
        detail::stream* stream_;
        bool readable_;
        bool writable_;
    };
}

#endif
