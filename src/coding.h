#pragma once
#include <string>
#include <vector>

#include "result.h"
#include "termbox2.h"

namespace mango {

constexpr uint32_t kSpaceChar = ' ';
constexpr uint32_t kReplacementChar = 0xFFFD;  // �
constexpr uint32_t kTabChar = '\t';
constexpr uint32_t kTildeChar = '~';
constexpr uint32_t kSlashChar = '/';

constexpr const char* kSpace = " ";
constexpr const char* kNewLine = "\n";
constexpr const char* kTab = "\t";
constexpr const char* kSlash = "/";

bool IsUtf8BeginByte(char b);

// Convert UTF-8 null-terminated byte sequence to UTF-32 codepoint.
//
// If `c` is an empty C string, return 0. `out` is left unchanged.
//
// If a null byte is encountered in the middle of the codepoint, return a
// negative number indicating how many bytes were processed. `out` is left
// unchanged.
//
// Otherwise, return byte length of codepoint (1-6).
// For utf-8 size, see
// https://stackoverflow.com/questions/9533258/what-is-the-maximum-number-of-bytes-for-a-utf-8-encoded-character
inline int Utf8ToUnicode(const char* in, uint32_t& out) {
    return tb_utf8_char_to_unicode(&out, in);
}

// Convert UTF-32 codepoint to UTF-8 null-terminated byte sequence.
// `out` must be char[7] or greater. Return byte length of codepoint (1-6).
inline int UnicodeToUtf8(uint32_t in, char* out) {
    return tb_utf8_unicode_to_char(out, in);
}


// We assume that
// 1. the file is pure utf-8 format, no error coding
// TODO:
// 1. support error uft-8 coding
// 2. support grapheme cluster
// 3. make it iterator

Result NextCharacterInUtf8(const std::string& str, int offset,
                     std::vector<uint32_t>& character, int& byte_len,
                     int& width);

Result PrevCharacterInUtf8(const std::string& str, int offset,
                     std::vector<uint32_t>& character, int& byte_len,
                     int& width);
}  // namespace mango