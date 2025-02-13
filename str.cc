// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "str.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cinttypes>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace str
{

std::intmax_t
toi(const std::string_view &str)
{
    const auto *b = str.data();
    const auto *e = b + str.size();
    std::intmax_t i = 0;
    std::from_chars(b, e, i);
    return i;
}

std::string
join(const std::string_view &sep, const std::vector<std::string_view> &strs)
{
    std::string out;
    std::string_view p;
    for (const auto &s : strs) {
        out.append(p);
        out.append(s);
        p = sep;
    }
    return out;
}

std::string
concat(const std::vector<std::string_view> &strs)
{
    return join("", strs);
}

std::string_view
ltrim(std::string_view s, const std::string_view ws)
{
    auto f = s.find_first_not_of(ws);
    if (f == s.npos)
        return {};
    s.remove_prefix(f);
    return s;
}

std::string_view
rtrim(std::string_view s, const std::string_view ws)
{
    auto l = s.find_last_not_of(ws);
    if (l == s.npos)
        return {};
    s.remove_suffix(s.length() - l - 1);
    return s;
}

std::string_view
trim(std::string_view s, const std::string_view ws)
{
    return rtrim(ltrim(s, ws));
}

bool
eq(const std::string_view &a, const std::string_view &b)
{
    return a == b;
}

bool
eqcase(const std::string_view &a, const std::string_view &b)
{
    if (a.length() != b.length())
        return false;
    for (const auto [ac, bc] : std::views::zip(a, b))
        if (tolower(ac) != tolower(bc))
            return false;
    return true;
}

bool
starteqcase(const std::string_view &s, const std::string_view &prefix)
{
    const auto start = s.substr(0, prefix.length());
    return eqcase(start, prefix);
}

// Returns true IFF s matches m.
bool
match(const std::string_view &s, const std::string_view &m)
{
    auto seplen = 1;
    auto pos = m.find('_');
    if (pos == m.npos) {
        pos = m.length();
        seplen = 0;
    }
    if (pos == 0)
        return s.empty();
    if (s.length() < pos || m.length() - seplen < s.length())
        return false;
    const auto sp = s.begin() + pos;
    const std::string_view sbefore(s.begin(), sp);
    const std::string_view safter(sp, s.end());
    const auto mp = m.begin() + pos;
    const std::string_view mbefore(m.begin(), mp);
    const std::string_view mafter(mp + seplen, mp + seplen + safter.length());
    return eqcase(sbefore, mbefore) && eqcase(safter, mafter);
}

bool
contains(const std::vector<std::string> &v, const std::string_view &key)
{
    const auto end = v.end();
    const auto it = std::find(v.begin(), end, key);
    return it != end;
}

bool
contains(const std::vector<std::string_view> &v, const std::string_view &key)
{
    const auto end = v.end();
    const auto it = std::find(v.begin(), end, key);
    return it != end;
}

std::vector<std::string_view>
split(const std::string_view &str, const std::string_view &sep,
    bool discard_empty)
{
    if (str.empty())
        return {};
    if (sep.empty())
        return {str};
    std::vector<std::string_view> v;
    std::size_t pos{}, ppos{};
    do {
        pos = str.find(sep, ppos);
        const auto end = pos == str.npos ? str.length() : pos;
        const auto len = end - ppos;
        const auto token = str.substr(ppos, len);
        if (!discard_empty || !token.empty())
            v.push_back(trim(token));
        ppos = pos + 1;
    } while (pos != str.npos);
    return v;
}

std::vector<std::string>
splits(const std::string_view &str, const std::string_view &sep,
    bool discard_empty)
{
    const auto v = str::split(str, sep, discard_empty);
    std::vector<std::string> sv;
    std::transform(v.begin(), v.end(), std::back_inserter(sv),
        [](auto v) { return std::string(v); });
    return sv;
}

// Removes surrounding quotes from a string.
std::string
unquote(const std::string_view &str)
{
    if (str.empty())
        return "";
    const auto q = str.front();
    if (q != '"' && q != '\'')
        return std::string(str);
    std::string out;
    for (auto it = str.cbegin() + 1; it != str.cend() && *it != q; ++it) {
        const auto next = it + 1;
        if (*it == '\\' && next != str.end() && *next == q)
            it = next;
        out.push_back(*it);
    }
    return out;
}

std::string &
lowercase(std::string &str)
{
    for (auto &c : str) c = tolower(c);
    return str;
}

std::string
lowercase(const std::string_view &str)
{
    std::string s(str);
    lowercase(s);
    return s;
}

// Returns a new string that is `s` with any of the given characters removed.
std::string
strip(const std::string_view &s, const std::string_view &chars)
{
    std::string out(s);
    std::erase_if(out, [&](char c) { return chars.find(c) != chars.npos; });
    return out;
}

} // namespace str
