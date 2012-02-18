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
        {}

        Buffer(const Buffer& c)
            : data_(c.data_)
        {}

        Buffer(const std::vector<char>& data)
            : data_(data)
        {}

        Buffer(const std::string& str)
            : data_(str.begin(), str.end())
        {}

        virtual ~Buffer()
        {}

    public:
        char& operator[](std::size_t idx) { return data_[idx]; }
        const char& operator[](std::size_t idx) const { return data_[idx]; }

        Buffer& operator =(const Buffer& c)
        {
            data_ = c.data_;
            return *this;
        }

    private:
        std::vector<char> data_;
    };
}

#endif
