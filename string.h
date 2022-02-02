// string.h
#pragma once

namespace theta
{

struct StringSpan
{
    StringSpan()
    {}

    StringSpan(const char* begin, const char* end)
        : _begin(begin)
        , _end(end)
    {}

    size_t getSize() const { return _end - _begin; }
    char const* getData() const { return _begin; }

    char const* _begin = nullptr;
    char const* _end = nullptr;
};

inline bool operator==(StringSpan const& left, StringSpan const& right)
{
    size_t size = left.getSize();
    if (size != right.getSize()) return false;

    return memcmp(left.getData(), right.getData(), size) == 0;
}

}
