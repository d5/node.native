#ifndef __HTTP_H__
#define __HTTP_H__

#include <sstream>
#include "base.h"
#include "detail.h"
#include "events.h"
#include "net.h"

namespace native
{
    namespace http
    {
        class IncomingMessage : public Stream
        {
        public:
            IncomingMessage(net::Socket* socket)
                : Stream(socket->stream(), true, false)
                , socket_(socket)
                , http_version_()
                , complete_(false)
                , url_()
                , method_()
                , status_code_(0)
            {}

            virtual ~IncomingMessage()
            {}

        public:
            // TODO: implement IncomingMessage::setEncoding()
            void setEncoding(const std::string& encoding)
            {}

            // these are inherited from Stream
            //void destroy(Exception exception) { socket_->destroy(exception); }
            //void pause() { socket_->pause(); }
            //void resume() { socket_->resume(); }

        private:
            net::Socket* socket_;
            std::string http_version_;
            bool complete_;
            std::string url_;
            std::string method_;
            int status_code_;
        };

        class OutgoingMessage : public Stream
        {
        public:
            OutgoingMessage()
                : Stream(nullptr, false, true)
                , header_str_()
                , header_sent_(false)
                , headers_()
                , has_body_(true)
                , should_keep_alive_(true)
                , last_(false)
                , chunked_encoding_(false)
                , use_chunked_encoding_by_default_(true)
            {}

            virtual ~OutgoingMessage()
            {}

        public:
            // these are inherited from Stream
            //void destroy(Exception exception) { socket_->destroy(exception); }

            void setHeader(const std::string& name, const std::string& value)
            {
                if(!header_str_.empty()) throw Exception("Can't set headers after they are sent.");

                headers_[name] = value;
            }

            std::string getHeader(const std::string& name, const std::string& default_value=std::string())
            {
                return headers_.get(name, default_value);
            }

            bool getHeader(const std::string& name, std::string& value)
            {
                return headers_.get(name, value);
            }

            void removeHeader(const std::string& name)
            {
                if(!header_str_.empty()) throw Exception("Can't remove headers after they are sent.");

                headers_.remove(name);
            }

        protected:
            void store_headers_(const std::string first_line)
            {
                bool sent_connection_header = false;
                bool sent_transfer_encoding_header = false;
                bool sent_content_length_header = false;
                bool sent_expect = false;

                std::string message_header = first_line;

                auto store = [&](const std::string& field, const std::string& value) {
                    message_header += field + ": " + value + "\r\n";

                    if(util::text::compare_no_case(field, "Connection"))
                    {
                        sent_connection_header = true;
                        if(util::text::compare_no_case(value, "close")) last_ = true;
                        else should_keep_alive_ = true;
                    }
                    else if(util::text::compare_no_case(field, "Transfer-Encoding"))
                    {
                        sent_transfer_encoding_header = true;
                        if(util::text::compare_no_case(value, "chunk")) chunked_encoding_ = true;
                    }
                    else if(util::text::compare_no_case(field, "Content-Length"))
                    {
                        sent_content_length_header = true;
                    }
                    else if(util::text::compare_no_case(field, "Expect"))
                    {
                        sent_expect = true;
                    }
                };

                for(auto h : headers_)
                {
                    store(h.first, h.second);
                }

                // keep-alive logic
                if(!sent_connection_header)
                {
                    // TODO: agent???
                    //if(should_keep_alive_ && (sent_connection_header || use_chunked_encoding_by_default_ || agent_))
                    if(should_keep_alive_ && (sent_connection_header || use_chunked_encoding_by_default_))
                    {
                        message_header += "Connection: keep-alive\r\n";
                    }
                    else
                    {
                        last_ = true;
                        message_header += "Connection: close\r\n";
                    }
                }

                if(!sent_connection_header && !sent_transfer_encoding_header)
                {
                    if(has_body_)
                    {
                        if(use_chunked_encoding_by_default_)
                        {
                            message_header += "Transfer-Encoding: chunked\r\n";
                            chunked_encoding_ = true;
                        }
                        else
                        {
                            last_ = true;
                        }
                    }
                    else
                    {
                       chunked_encoding_ = false;
                    }
                  }

                  header_str_ = message_header + "\r\n";
                  header_sent_ = false;

                  // wait until the first body chunk, or close(), is sent to flush,
                  // UNLESS we're sending Expect: 100-continue.
                  if(sent_expect) send_(Buffer(""));
            }

        private:
            virtual void implicit_header_() {}

            void send_(const Buffer& data)
            {
            }

        protected:
            std::string header_str_;
            bool header_sent_;
            util::idict headers_;
            int status_code_;
            bool has_body_;
            bool should_keep_alive_;
            bool last_;
            bool chunked_encoding_;
            bool use_chunked_encoding_by_default_;
        };

        class ServerResponse : public OutgoingMessage
        {
        public:
            ServerResponse()
                : OutgoingMessage()
                , expect_continue_(false)
                , sent100_(false)
            {}

        public:
            void writeHead(int statusCode)
            {
                status_code_ = statusCode;
                std::string status_text(detail::http_status_text(status_code_));

                std::stringstream status_line;
                status_line << "HTTP/1.1 " << status_code_ << " " << status_text << "\r\n";

                if(status_code_ == 204 || status_code_ == 304
                    || (100 <= status_code_ && status_code_ <= 199)) {
                    has_body_ = false;
                }

                if(expect_continue_ && !sent100_)
                {
                    should_keep_alive_ = false;
                }

                store_headers_(status_line.str());
            }

        private:
            virtual void implicit_header_()
            {
                writeHead(status_code_);
            }

        private:
            bool expect_continue_;
            bool sent100_;
        };

        class ClientRequest : public OutgoingMessage
        {};


    }
}

#endif
