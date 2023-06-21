/////////////////////////////////////////////////////////////////////////////
// Name:        char_utils.hpp
// Purpose:     Misc. char functions
// Author:      Thomas Figueroa
// Created:     2023-06-20
// Copyright:   Thomas Figueroa
// Licence:     MIT
/////////////////////////////////////////////////////////////////////////////


#ifndef __CHAR_UTILS_HPP__
#define __CHAR_UTILS_HPP__

#include <concepts>
#include <cstddef>

inline constexpr bool isAlphaUpper(char ch)
{
    return 'A' == ch ||
           'B' == ch ||
           'C' == ch ||
           'D' == ch ||
           'E' == ch ||
           'F' == ch ||
           'G' == ch ||
           'H' == ch ||
           'I' == ch ||
           'J' == ch ||
           'K' == ch ||
           'L' == ch ||
           'M' == ch ||
           'N' == ch ||
           'O' == ch ||
           'P' == ch ||
           'Q' == ch ||
           'R' == ch ||
           'S' == ch ||
           'T' == ch ||
           'U' == ch ||
           'V' == ch ||
           'W' == ch ||
           'X' == ch ||
           'Y' == ch ||
           'Z' == ch;
}

inline constexpr bool isAlphaLower(char ch)
{
    return 'a' == ch ||
           'b' == ch ||
           'c' == ch ||
           'd' == ch ||
           'e' == ch ||
           'f' == ch ||
           'g' == ch ||
           'h' == ch ||
           'i' == ch ||
           'j' == ch ||
           'k' == ch ||
           'l' == ch ||
           'm' == ch ||
           'n' == ch ||
           'o' == ch ||
           'p' == ch ||
           'q' == ch ||
           'r' == ch ||
           's' == ch ||
           't' == ch ||
           'u' == ch ||
           'v' == ch ||
           'w' == ch ||
           'x' == ch ||
           'y' == ch ||
           'z' == ch;
}

inline constexpr bool isAlpha(char ch)
{
    return isAlphaUpper(ch) || isAlphaLower(ch);
}

inline constexpr bool isDigit(char ch)
{
    return '0' == ch ||
           '1' == ch ||
           '2' == ch ||
           '3' == ch ||
           '4' == ch ||
           '5' == ch ||
           '6' == ch ||
           '7' == ch ||
           '8' == ch ||
           '9' == ch;
}

inline constexpr bool isHexLetterUpper(char ch)
{
    return 'A' == ch ||
           'B' == ch ||
           'C' == ch ||
           'D' == ch ||
           'E' == ch ||
           'F' == ch;
}

inline constexpr bool isHexLetterLower(char ch)
{
    return 'a' == ch ||
           'b' == ch ||
           'c' == ch ||
           'd' == ch ||
           'e' == ch ||
           'f' == ch;
}

inline constexpr bool isHexLetter(char ch)
{
    return isHexLetterUpper(ch) || isHexLetterLower(ch);
}

inline constexpr bool isHex(char ch)
{
    return isHexLetter(ch) || isDigit(ch);
}

inline constexpr bool isWhitespace(char ch)
{
    return ch == ' '  ||
           ch == '\f' ||
           ch == '\n' ||
           ch == '\r' ||
           ch == '\t' ||
           ch == '\v';
};

inline constexpr bool isPunct(char ch)
{
    return '!'  == ch ||
           '"'  == ch ||
           '#'  == ch ||
           '$'  == ch ||
           '%'  == ch ||
           '&'  == ch ||
           '\'' == ch ||
           '('  == ch ||
           ')'  == ch ||
           '*'  == ch ||
           '+'  == ch ||
           ','  == ch ||
           '-'  == ch ||
           '.'  == ch ||
           '/'  == ch ||
           ':'  == ch ||
           ';'  == ch ||
           '<'  == ch ||
           '='  == ch ||
           '>'  == ch ||
           '?'  == ch ||
           '@'  == ch ||
           '['  == ch ||
           '\\' == ch ||
           ']'  == ch ||
           '^'  == ch ||
           '_'  == ch ||
           '`'  == ch ||
           '{'  == ch ||
           '|'  == ch ||
           '}'  == ch ||
           '~'  == ch; 
}

inline constexpr bool isAlNum(char ch)
{
    return isDigit(ch) || isAlpha(ch);
}

inline constexpr char ToUpperCh(char ch)
{
    switch(ch)
    {
        case 'a': return 'A';
        case 'b': return 'B';
        case 'c': return 'C';
        case 'd': return 'D';
        case 'e': return 'E';
        case 'f': return 'F';
        case 'g': return 'G';
        case 'h': return 'H';
        case 'i': return 'I';
        case 'j': return 'J';
        case 'k': return 'K';
        case 'l': return 'L';
        case 'm': return 'M';
        case 'n': return 'N';
        case 'o': return 'O';
        case 'p': return 'P';
        case 'q': return 'Q';
        case 'r': return 'R';
        case 's': return 'S';
        case 't': return 'T';
        case 'u': return 'U';
        case 'v': return 'V';
        case 'w': return 'W';
        case 'x': return 'X';
        case 'y': return 'Y';
        case 'z': return 'Z';
        default:  return ch;
    }
}

inline constexpr char ToLowerCh(char ch)
{
    switch(ch)
    {
        case 'A': return 'a';
        case 'B': return 'b';
        case 'C': return 'c';
        case 'D': return 'd';
        case 'E': return 'e';
        case 'F': return 'f';
        case 'G': return 'g';
        case 'H': return 'h';
        case 'I': return 'i';
        case 'J': return 'j';
        case 'K': return 'k';
        case 'L': return 'l';
        case 'M': return 'm';
        case 'N': return 'n';
        case 'O': return 'o';
        case 'P': return 'p';
        case 'Q': return 'q';
        case 'R': return 'r';
        case 'S': return 's';
        case 'T': return 't';
        case 'U': return 'u';
        case 'V': return 'v';
        case 'W': return 'w';
        case 'X': return 'x';
        case 'Y': return 'y';
        case 'Z': return 'z';
        default:  return ch;
    }
}

template<char c>
concept Hexable = requires
{
    requires(('A' <= c && c <= 'F') ||
             ('0' <= c && c <= '9') ||
             ('a' <= c && c <= 'f'));
};

inline constexpr std::byte HexCharToDec(std::byte C)
{
    const auto asChar = static_cast<char>(C);

    if('A' <= asChar && asChar <= 'F')
    {
        return static_cast<std::byte>(asChar - char{55});
    }
    else if('a' <= asChar && asChar <= 'f')
    {
        return static_cast<std::byte>(asChar - char{87});
    }
    else // '0' - '9
    {
        return static_cast<std::byte>(asChar - char{48});
    }
}

#endif // __CHAR_UTILS_HPP__
