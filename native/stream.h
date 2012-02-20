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
            , is_stdio_(false)
            , pipe_()
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

        virtual Stream* pipe(Stream* destination, const util::dict& options)
        {
            pipe_ = std::shared_ptr<pipe_context>(new pipe_context(this, destination, options));
            assert(pipe_);

            return destination;
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
        struct pipe_context : public detail::object
        {
            pipe_context(Stream* source, Stream* destination, const util::dict& options)
                : detail::object(3)
                , source_(source)
                , destination_(destination)
                , did_on_end_(false)
            {
                assert(source_ && destination_);

                // whenever data is available from source, copy it destination
                source_on_data_ = source_->on<ev::data>([&](const Buffer& chunk){
                    if(destination_->writable())
                    {
                        if(!destination_->write(chunk))
                        {
                            // destination is full: pause source
                            // TODO: some resources might not be supporting pause()
                            source_->pause();
                        }
                    }
                });

                // when the destination gets writable again, resume reading
                dest_on_drain_ = destination_->on<ev::drain>([&](){
                    // TODO: some resources might not be supporting resume()
                    if(source_->readable()) source_->resume();
                });

                source_on_error_ = source_->on<ev::error>([&](Exception exception){ on_error(exception); });
                dest_on_error_ = destination_->on<ev::error>([&](Exception exception){ on_error(exception); });

                // assign "end", "close" event callback
                if(!destination_->is_stdio_ && !options.compare("end", "false"))
                {
                    source_on_end_ = source_->on<ev::end>([&](){ on_end(); });
                    source_on_close_ = source_->on<ev::close>([&](){ on_close(); });
                }

                source_on_end_clenaup_ = source_->on<ev::end>([&](){ cleanup(); });
                source_on_close_clenaup_ = source_->on<ev::close>([&](){ cleanup(); });

                dest_on_end_clenaup_ = destination_->on<ev::end>([&](){ cleanup(); });
                dest_on_close_clenaup_ = destination_->on<ev::close>([&](){ cleanup(); });

                destination_->emit<ev::pipe>(source_);
            }

            void on_error(Exception exception)
            {
                cleanup();
            }

            void on_close()
            {
                if(did_on_end_) return;
                did_on_end_ = true;

                cleanup();

                destination_->destroy();
            }

            void on_end()
            {
                if(did_on_end_) return;
                did_on_end_ = true;

                cleanup();

                destination_->end();
            }

            void cleanup()
            {
                source_->removeListener<ev::data>(source_on_data_);
                destination_->removeListener<ev::drain>(dest_on_drain_);

                source_->removeListener<ev::end>(source_on_end_);
                source_->removeListener<ev::close>(source_on_close_);

                source_->removeListener<ev::error>(source_on_error_);
                destination_->removeListener<ev::error>(dest_on_error_);

                source_->removeListener<ev::end>(source_on_end_clenaup_);
                source_->removeListener<ev::close>(source_on_close_clenaup_);

                destination_->removeListener<ev::end>(dest_on_end_clenaup_);
                destination_->removeListener<ev::close>(dest_on_close_clenaup_);
            }

            Stream* source_;
            Stream* destination_;

            void* source_on_data_;
            void* dest_on_drain_;
            void* source_on_end_;
            void* source_on_close_;
            void* source_on_error_;
            void* dest_on_error_;
            void* source_on_end_clenaup_;
            void* source_on_close_clenaup_;
            void* dest_on_end_clenaup_;
            void* dest_on_close_clenaup_;

            bool did_on_end_;
        };

    private:
        detail::stream* stream_;
        bool readable_;
        bool writable_;

        bool is_stdio_;
        std::shared_ptr<pipe_context> pipe_;
    };
}

#endif

