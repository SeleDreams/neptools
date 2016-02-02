#ifndef FIXED_STRING_HPP
#define FIXED_STRING_HPP
#pragma once

#include <cstring>
#include <string>
#include <algorithm>
#include <boost/operators.hpp>

template <size_t N>
class FixedString :
    boost::totally_ordered<FixedString<N>>,
    boost::totally_ordered<FixedString<N>, const char*>
{
public:
    FixedString() = default;
    explicit FixedString(const char* cstr)
    { strncpy(str, cstr, N-1); str[N-1] = '\0'; }
    explicit FixedString(const std::string& sstr)
    {
        auto copied = std::min(sstr.size(), N-1);
        memcpy(str, sstr.data(), copied);
        memset(str+copied, 0, N-copied);
    }

    bool is_valid() const noexcept
    {
        auto len = strnlen(str, N-1);
        for (size_t i = len; i < N; ++i)
            if (str[i] != '\0')
                return false;
        return true;
    }

    char& operator[](size_t i) noexcept { return str[i]; }
    const char& operator[](size_t i) const noexcept { return str[i]; }

    const char* data() const noexcept { return str; }
    const char* c_str() const noexcept { return str; }

    bool empty() const noexcept { return str[0] == 0; }
    size_t size() const noexcept { return strnlen(str, N); }
    size_t length() const noexcept { return strnlen(str, N); }
    size_t max_size() const noexcept { return N-1; }

    bool operator==(const FixedString& o) const noexcept
    { return strncmp(str, o.str, N) == 0; }
    bool operator<(const FixedString& o) const noexcept
    { return strncmp(str, o.str, N) < 0; }

    bool operator==(const char* o) const noexcept
    { return strcmp(str, o) == 0; }
    bool operator<(const char* o) const noexcept
    { return strcmp(str, o) < 0; }

    template <size_t M>
    bool operator==(const FixedString<M>& o) const noexcept
    { return strcmp(str, o.str) == 0; }
    template <size_t M>
    bool operator<=(const FixedString<M>& o) const noexcept
    { return strcmp(str, o.str) <= 0; }
private:
    char str[N];
};

template <size_t N>
inline std::ostream& operator<<(std::ostream& os, const FixedString<N>& str)
{ return os << str.data(); }

/*
template <size_t N>
inline std::istream& operator>>(std::istream& is, FixedString<N>& str)
{ is.width(N-1); return is >> &str[0]; }
*/

#endif