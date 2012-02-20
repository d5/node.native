#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#include "base.h"

namespace native
{
    // TODO: implement Buffer class
    class Buffer
    {
    public:
        Buffer()
            : data_()
            , encoding_(detail::et_utf8)
        {}

        Buffer(const Buffer& c)
            : data_(c.data_)
            , encoding_(c.encoding_)
        {}

        explicit Buffer(const std::vector<char>& data)
            : data_(data)
            , encoding_(detail::et_utf8)
        {}

        explicit Buffer(const std::string& str)
            : data_(str.begin(), str.end())
            , encoding_(detail::et_ascii)
        {}

        explicit Buffer(const std::string& str, const std::string& encoding)
            : data_(str.begin(), str.end())
            , encoding_(detail::get_encoding_code(encoding))
        {
            // TODO: implement Buffer(const std::string& str, const std::string& encoding)
            // ...
        }

        explicit Buffer(const char* data, std::size_t length)
            : data_(data, data + length)
            , encoding_(detail::et_utf8)
        {}

        Buffer(Buffer&& c)
            : data_(std::move(c.data_))
            , encoding_(c.encoding_)
        {}

        explicit Buffer(std::nullptr_t)
            : data_()
            , encoding_(detail::et_none)
        {}

        virtual ~Buffer()
        {}

        char& operator[](std::size_t idx) { return data_[idx]; }
        const char& operator[](std::size_t idx) const { return data_[idx]; }

        Buffer& operator =(const Buffer& c)
        {
            data_ = c.data_;
            encoding_ = c.encoding_;
            return *this;
        }

        Buffer& operator =(Buffer&& c)
        {
            data_ = std::move(c.data_);
            encoding_ = c.encoding_;
            return *this;
        }

        char* base() { return &data_[0]; }
        const char* base() const { return &data_[0]; }

        std::size_t size() const { return data_.size(); }

        Buffer slice(std::size_t start, std::size_t end)
        {
            return Buffer(std::vector<char>(data_.begin()+start, data_.begin()+end));
        }

    private:
        std::vector<char> data_;
        int encoding_;
    };
}

#endif
