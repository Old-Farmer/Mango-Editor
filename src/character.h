#pragma once
#include <string>
#include <vector>

#include "result.h"
#include "termbox2.h"
#include "utf8proc.h"
#include "utils.h"

namespace mango {

using Codepoint = utf8proc_int32_t;

constexpr Codepoint kSpaceChar = ' ';           // Use this to avoid confusion
constexpr Codepoint kReplacementChar = 0xFFFD;  // �
constexpr int kReplacementCharWidth = 1;

constexpr const char* kSpace = " ";
constexpr const char* kReplacement = "�";

// A unicode grapheme. Any op on a user-perceived character should use this if
// you can avoid grapheme concept.

// It is only safe to determine a codepoint as a grapheme if it is a ascii
// control code, otherwise it is always safer to use this facility.

// In this codebase, we may sometimes use ascii(on single char or on single
// codepoint) to detect strings content if they are not number or english alpha,
// like ' ', '(', '[', '{', '\"', '\'', '/', '\\'. It's quite ok.

// TODO: single codepoint optimization.
class Character {
   public:
    bool Ascii(char& c) const {
        if (codepoints_.size() == 1 && codepoints_[0] <= CHAR_MAX) {
            c = static_cast<char>(codepoints_[0]);
            return true;
        }
        return false;
    }

    void Push(Codepoint codepoint) { codepoints_.push_back(codepoint); }

    void Clear() { codepoints_.clear(); }

    const Codepoint* Codepoints() const {
        MGO_ASSERT(!codepoints_.empty());
        return codepoints_.data();
    }

    // Codepoint count
    size_t CodePointCount() const { return codepoints_.size(); }

    int Width();

    std::string ToString();

   private:
    std::vector<Codepoint> codepoints_;
};

bool IsUtf8BeginByte(char b);

// decode str to a codepoint
// if success, kOk return and len will be set to the byte consumed, out will be
// the codepoint.
// otherwise, kInvalidCoding will return.
inline Result Utf8ToUnicode(const char* in, size_t len, int& byte_eat,
                            Codepoint& out) {
    MGO_ASSERT(len != 0);
    if ((byte_eat = utf8proc_iterate(
             reinterpret_cast<const utf8proc_uint8_t*>(in), len, &out)) < 0) {
        return kInvalidCoding;
    }
    return kOk;
}

// Encode a codepoint, out buf must longer than 4 bytes
// On success, return encoded str len
// otherwise, return 0;
inline int UnicodeToUtf8(uint32_t in, char* out) {
    return utf8proc_encode_char(in, reinterpret_cast<utf8proc_uint8_t*>(out));
}

int CharacterWidth(const Codepoint* codepoints, size_t cnt);

bool CheckUtf8Valid(std::string_view str);

// We assume that
// 1. the str is pure utf-8 format, no error coding, because we check errors for
// all outer source.
// TODO:
// 1. make it iterator

// str[offset] must be a character beginnig byte, otherwise behavior is
// undefined.
// offset shouldn't >= str.size().
// Current only return kOk.
Result ThisCharacter(std::string_view str, int64_t offset, Character& character,
                     int& byte_len);

// Make sure that str[offset] must be a character beginnig byte.
// offset shouldn't <= 0.
// Current only return kOk
Result PrevCharacter(std::string_view str, int64_t offset, Character& character,
                     int& byte_len);

// A word contains isalnum + _: just ascii character.
// TODO: support configuration.
bool IsWordCharacter(char c);

// next word offset will set to the next word begin
// Current only return kOk
Result NextWordBegin(const std::string& str, size_t offset,
                     size_t& next_word_offset);

// next word offset will set to the this/next word end
// Current only return kOk
Result WordEnd(const std::string& str, size_t offset, bool one_more_character,
               size_t& next_word_end_offset);

// next word offset will set to the this/prev word end
// on success, return kOk;
// if Not found, retrun kNotExist and set prev_word_offset to 0
Result WordBegin(const std::string& str, size_t offset,
                 size_t& prev_word_offset);

// If c is the open part of a pair, return true and the close part.
inline std::pair<bool, char> IsPairOpen(char c) {
    switch (c) {
        case '{':
            return {true, '}'};
        case '[':
            return {true, ']'};
        case '(':
            return {true, ')'};
        case '\'':
        case '\"':
            return {true, c};
    }
    return {false, -1};
}

// If c is the open or close part of a pair.
inline bool IsPair(char c) {
    return c == '(' || c == '{' || c == '[' || c == ')' || c == '}' ||
           c == ']' || c == '\'' || c == '\"';
}

// Can open and close pair together ?
inline bool IsPair(char open, char close) {
    switch (open) {
        case '(':
            return close == ')';
        case '{':
            return close == '}';
        case '[':
            return close == ']';
        case '\'':
        case '\"':
            return close == open;
    }
    return false;
}

size_t StringWidth(const std::string& str);

}  // namespace mango