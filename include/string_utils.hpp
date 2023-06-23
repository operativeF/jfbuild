/////////////////////////////////////////////////////////////////////////////
// Name:        string_utils.hpp
// Purpose:     Misc. string functions
// Author:      Thomas Figueroa
// Created:     2023-06-20
// Copyright:   Thomas Figueroa
// Licence:     MIT
/////////////////////////////////////////////////////////////////////////////


#ifndef __STRING_UTILS_HPP__
#define __STRING_UTILS_HPP__

#include "char_utils.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

// Modifying type string utilities.

inline constexpr std::size_t ReplaceAll(std::string& instr, std::string_view candidate, std::string_view replacement)
{
    std::size_t count{ 0 };

    if(candidate.empty() || replacement.empty())
        return 0;

    for (std::string::size_type pos{};
        instr.npos != (pos = instr.find(candidate.data(), pos, candidate.length()));
        pos += replacement.length(), ++count)
    {
        instr.replace(pos, candidate.length(), replacement.data(), replacement.length());
    }

    return count;
}

// Trims all space leading and following, but not in the middle.
template<typename R>
inline constexpr void TrimAllSpace(R& str)
{
    auto it1 = std::ranges::find_if_not(str, isWhitespace);

    str.erase(str.begin(), it1);

    auto it2 = std::ranges::find_if_not(std::ranges::reverse_view(str), isWhitespace);

    str.erase(it2.base(), str.end());
}

template<typename R>
inline constexpr void TrimLeadingSpace(R& str)
{
    auto it = std::ranges::find_if_not(str, isWhitespace);

    str.erase(str.begin(), it);
}

template<typename R>
inline constexpr void TrimTrailingSpace(R& str)
{
    auto it = std::ranges::find_if_not(std::ranges::reverse_view(str), isWhitespace);

    str.erase(it.base(), str.end());
}

template<typename R>
inline constexpr void ToUpper(R& str)
{
    std::ranges::transform(str, str.begin(), [](auto c) noexcept { return ToUpperCh(c); });
}

template<typename R>
inline constexpr void ToLower(R& str)
{
    std::ranges::transform(str, str.begin(), [](auto c) noexcept { return ToLowerCh(c); });
}

inline constexpr void EraseSubstr(std::string& str, std::string_view subToErase)
{
    std::size_t pos{std::string::npos};

    while((pos = str.find(subToErase)) != std::string::npos)
    {
        str.erase(pos, subToErase.length());
    }
}


// TODO: Not valid for unicode strings. (ToUpperCh function).
// Need to detect underlying char type for return type deduction.
template<typename R>
inline constexpr std::string ToUpperCopy(R&& str)
{
    auto tview = std::ranges::views::transform(str, [](auto c) noexcept { return ToUpperCh(c); });

    return {tview.begin(), tview.end()};
}

template<typename R>
inline constexpr std::string ToLowerCopy(R&& str)
{
    auto tview = std::ranges::views::transform(str, [](auto c) noexcept { return ToLowerCh(c); });

    return {tview.begin(), tview.end()};
}

inline constexpr std::vector<std::string> StrSplit(std::string_view strView, char delim)
{
    std::vector<std::string> output;

    size_t first{0};

    while (first < strView.size())
    {
        const auto second = strView.find_first_of(delim, first);

        if (first != second)
            output.emplace_back(strView.substr(first, second - first));

        if (second == std::string_view::npos)
            break;

        first = second + 1;
    }

    return output;
}

// TODO: could be improved, but is sufficient for string conversion.
inline constexpr std::vector<std::string> StrSplitEscape(std::string_view strView, char delim, char escape)
{
    std::vector<std::string> output;

    std::string s;

    for(std::string_view::iterator i = strView.begin(); i != strView.end();++i)
    {
        const auto ch = *i;

        if(ch != delim && ch != escape)
        {
            s += ch;
        }
        
        if(ch == escape)
        {
            if(*(i + 1) == delim)
            {
                std::advance(i, 1);
                s += delim;
            }
            else
            {
                s += ch;
            }
        }
        
        if(ch == delim || i == std::ranges::prev(strView.end(), 1))
        {
            output.emplace_back(s);
            s.clear();

            // special case of empty delimiter at end
            if(ch == delim && i == (strView.end() - 1))
            {
                output.emplace_back("");
            }
        }
    }

    return output;
}

// Join a '\0' delimited array of chars into a vector of strings.
inline std::vector<std::string> JoinChArray(std::span<const char> chSpan)
{
    std::vector<std::string> vec;

    for(auto chIter = chSpan.begin(); chIter != chSpan.end(); chIter = std::ranges::next(chIter))
    {
        auto chBeginIter = chIter;

        while(*chIter != '\0')
        {
            chIter = std::ranges::next(chIter);
        }

        vec.emplace_back(chBeginIter, chIter);
    }

    return vec;
}

// FIXME: ICE VS2022 17.2
// [build] C:\dev\vanir\3rdparty\fmt\include\fmt\core.h(1915,1): fatal error C1001: Internal compiler error.
// [C:\dev\vanir\build\modules\nilbase.vcxproj]
// [build]   (compiler file 'msc1.cpp', line 1691)
// Delimit strings from a span of them.
// std::string JoinStrings(std::span<std::string> strSpan, char delim)
// {
//     return stfmt::format("{}", std::join(strSpan, std::string{delim}));
// }

// Like JoinStrings, except there is an
// escape character inserted before a delimiter that is found in a string.
inline std::string JoinStringsEsc(std::span<const std::string> strSpan, char delim, std::string_view esc)
{
    std::string str;

    const auto escBack = fmt::format("{}{}", esc, delim);

    for(auto&& name : strSpan)
    {
        for(auto&& ch : name)
        {
            if(ch != delim)
            {
                str += ch;
            }
            else
            {
                str += escBack;
            }
        }

        str += delim;
    }

    str.pop_back(); // Remove extra delimiter
    return str;
}

inline int CmpNoCaseN(std::string_view strViewA, std::string_view strViewB, std::size_t count)
{
    const auto nA = strViewA.size();
    const auto nB = strViewB.size();

    std::string strA(nA, '0');
    std::string strB(nB, '0');

    std::ranges::transform(strViewA, strA.begin(), [](auto c) noexcept { return ToLowerCh(c); });
    std::ranges::transform(strViewB, strB.begin(), [](auto c) noexcept { return ToLowerCh(c); });

    return strA.compare(0, count, strB);
}

inline int CmpNoCase(std::string_view strViewA, std::string_view strViewB)
{
    const auto nA = strViewA.size();
    const auto nB = strViewB.size();

    std::string strA(nA, '0');
    std::string strB(nB, '0');

    std::ranges::transform(strViewA, strA.begin(), [](auto c) noexcept { return ToLowerCh(c); });
    std::ranges::transform(strViewB, strB.begin(), [](auto c) noexcept { return ToLowerCh(c); });

    return strA.compare(strB);
}

inline constexpr bool IsSameAsCase(std::string_view strViewA, std::string_view strViewB) noexcept
{
    return strViewA == strViewB;
}

inline bool IsSameAsNoCase(std::string_view strViewA, std::string_view strViewB)
{
    return CmpNoCase(strViewA, strViewB) == 0;
}

template<typename R>
[[nodiscard]] inline constexpr std::string StripLeadingSpace(R&& str)
{
    auto it1 = std::ranges::find_if_not(str, isWhitespace);

    return {it1, str.end()};
}

template<typename R>
[[nodiscard]] inline constexpr std::string StripTrailingSpace(R&& str)
{
    auto it2 = std::ranges::find_if_not(std::ranges::reverse_view(str), isWhitespace);

    return {str.begin(), it2.base()};
}

template<typename R>
[[nodiscard]] inline constexpr std::string StripAllSpace(R&& str)
{
    auto it1 = std::ranges::find_if_not(str, isWhitespace);
    auto it2 = std::ranges::find_if_not(std::ranges::reverse_view(str), isWhitespace);

    return {it1, it2.base()};
}

inline bool IsSameAs(std::string_view strViewA, std::string_view strViewB, bool bCase)
{
    if(bCase)
    {
        return IsSameAsCase(strViewA, strViewB);
    }
    else
    {
        return IsSameAsNoCase(strViewA, strViewB);
    }
}

[[nodiscard]] inline constexpr std::string BeforeFirst(std::string_view strView, std::string_view strFirst, size_t pos = 0)
{
    const auto n = strView.find(strFirst, pos);

    if(n != std::string_view::npos)
        return std::string(strView.substr(0, n));
    
    return std::string(strView);
}

[[nodiscard]] inline constexpr std::string BeforeFirst(std::string_view strView, const char ch, size_t pos = 0)
{
    const auto n = strView.find(ch, pos);

    if(n != std::string_view::npos)
        return std::string(strView.substr(0, n));
    
    return std::string(strView);
}

[[nodiscard]] inline constexpr std::string BeforeFirst(std::string_view strView, const char* const chs, size_t pos = 0)
{
    return BeforeFirst(strView, std::string_view(chs), pos);
}

[[nodiscard]] inline constexpr std::string AfterFirst(std::string_view strView, std::string_view strAfter, size_t pos = 0)
{
    const auto n = strView.find(strAfter, pos);

    if(n != std::string_view::npos)
        return std::string(strView.substr(n + 1, strView.size()));
    
    return {};
}

[[nodiscard]] inline constexpr std::string AfterFirst(std::string_view strView, const char ch, size_t pos = 0)
{
    const auto n = strView.find(ch, pos);

    if(n != std::string_view::npos)
        return std::string(strView.substr(n + 1, strView.size()));
    
    return {};
}

[[nodiscard]] inline constexpr std::string AfterFirst(std::string_view strView, const char* const chs, size_t pos = 0)
{
    return AfterFirst(strView, std::string_view(chs), pos);
}

[[nodiscard]] inline constexpr std::string BeforeLast(std::string_view strView, std::string_view strBefore, size_t pos = std::string_view::npos)
{
    const auto n = strView.rfind(strBefore, pos);

    if(n != std::string_view::npos)
        return std::string(strView.substr(0, n));

    return {};
}

[[nodiscard]] inline constexpr std::string BeforeLast(std::string_view strView, const char ch, size_t pos = std::string_view::npos)
{
    const auto n = strView.rfind(ch, pos);

    if(n != std::string_view::npos)
        return std::string(strView.substr(0, n));

    return {};
}

[[nodiscard]] inline constexpr std::string BeforeLast(std::string_view strView, const char* const chs, size_t pos = std::string_view::npos)
{
    return BeforeLast(strView, std::string_view(chs), pos);
}


// TODO: Do we really want to return the whole input string if it fails to find anything?
[[nodiscard]] inline constexpr std::string AfterLast(std::string_view strView, std::string_view strLast, size_t pos = std::string_view::npos)
{
    const auto n = strView.rfind(strLast, pos);

    if(n != std::string_view::npos)
        return std::string{strView.substr(n + 1, strView.size())};

    return std::string{strView};
}

[[nodiscard]] inline constexpr std::string AfterLast(std::string_view strView, const char ch, size_t pos = std::string_view::npos)
{
    const auto n = strView.rfind(ch, pos);

    if(n != std::string_view::npos)
        return std::string{strView.substr(n + 1, strView.size())};

    return std::string{strView};
}

[[nodiscard]] inline constexpr std::string AfterLast(std::string_view strView, const char* const chs, size_t pos = std::string_view::npos)
{
    return AfterLast(strView, std::string_view{chs}, pos);
}

[[nodiscard]] inline constexpr bool EndsWith(std::string_view strView, std::string_view suffix, std::string& beforeSuffix)
{
    const auto pos = strView.find(suffix, strView.size() - suffix.size());

    if(pos != std::string::npos)
    {
        beforeSuffix = strView.substr(0, pos);

        return true;
    }

    return false;
}

[[nodiscard]] inline constexpr bool StartsWith(std::string_view strView, std::string_view prefix, std::string& afterStart)
{
    const auto pos = strView.rfind(prefix, 0);

    if(pos != std::string::npos)
    {
        afterStart = strView.substr(prefix.size());

        return true;
    }

    return false;
}

[[nodiscard]] inline constexpr bool StartsWith(std::string_view strView, std::string_view prefix)
{
    const auto pos = strView.rfind(prefix, 0);

    if(pos != std::string::npos)
    {
        return true;
    }

    return false;
}

[[nodiscard]] inline constexpr bool Contains(std::string_view strView, std::string_view strToFind) noexcept
{
    return strView.find(strToFind, 0) != std::string_view::npos;
}

// FIXME: Replace template with span
template<typename... Cs>
[[nodiscard]] inline constexpr bool ContainsAnyOf(std::string_view strView, Cs&&... cs) noexcept
{
    return (Contains(strView, cs) || ...);
}

// FIXME: Not valid for unicode strings
[[nodiscard]] inline constexpr bool ContainsNoCase(std::string_view strView, std::string_view strToFind)
{
    return std::ranges::search(strView, strToFind, {}, ToLowerCh, ToLowerCh).begin() != strView.end();
}

[[nodiscard]] inline bool IsMatch(std::string_view input,
                                  std::string_view pattern) {
  if (std::empty(pattern)) {
    return std::empty(input);
  }

  if (std::empty(input)) {
    return pattern[0] == '*' ? IsMatch(input, pattern.substr(1)) : false;
  }

  if (pattern[0] != '?' && pattern[0] != '*' && pattern[0] != input[0]) {
    return false;
  }

  if (pattern[0] == '*') {
    for (decltype(std::size(input)) i = 0u; i <= std::size(input); ++i) {
      if (IsMatch(input.substr(i), pattern.substr(1))) {
        return true;
      }
    }
    return false;
  }

  return IsMatch(input.substr(1), pattern.substr(1));
}

template <class TPattern, class TStr>
[[nodiscard]] inline constexpr std::vector<TStr> Match(const TPattern& pattern, const TStr& str) {
  std::vector<TStr> groups{};
  auto pi = 0u;
  auto si = 0u;

  const auto matcher = [&](char b, char e, char c = 0) {
    const auto match = si;
    while (str[si] && str[si] != b && str[si] != c) {
      ++si;
    }
    groups.emplace_back(str.substr(match, si - match));
    while (pattern[pi] && pattern[pi] != e) {
      ++pi;
    }
    pi++;
  };

  while (pi < std::size(pattern) && si < std::size(str)) {
    if (pattern[pi] == '\'' && str[si] == '\'' && pattern[pi + 1] == '{') {
      ++si;
      matcher('\'', '}');
    } else if (pattern[pi] == '{') {
      matcher(' ', '}', ',');
    } else if (pattern[pi] != str[si]) {
      return {};
    }
    ++pi;
    ++si;
  }

  if (si < str.size() || pi < std::size(pattern)) {
    return {};
  }

  return groups;
}

#endif // __STRING_UTILS_HPP__