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

// A unicode grapheme. Any op on a user-perceived character should use this,
// e.g. Parsing users' buffers.

// It is only safe to determine a codepoint as a grapheme if it is a ascii
// control code, otherwise it is always safer to use this facility.

// In this codebase, we sometimes use ascii(single char or single
// codepoint) to detect strings content if they are not number, english alpha,
// '#', '*', like ' ', '(', '[', '{', '\"', '\'', '/', '\\', and treat them as
// graphemes.

class Character {
   public:
    bool Ascii(char& c) const {
        if (codepoints_cnt_ == 1 && codepoint_ <= CHAR_MAX) {
            c = static_cast<char>(codepoint_);
            return true;
        }
        return false;
    }

    void Push(Codepoint codepoint) {
        if (codepoints_cnt_++ == 0) {
            codepoint_ = codepoint;
        } else if (codepoints_cnt_++ == 1) {
            codepoints_.resize(2);
            codepoints_[0] = codepoint_;
            codepoints_[1] = codepoint;
        } else {
            codepoints_.push_back(codepoint);
        }
    }

    // Explicitly set codepoint
    void Set(Codepoint codepoint) {
        codepoint_ = codepoint;
        codepoints_cnt_ = 1;
    }

    void Clear() {
        if (codepoints_cnt_ > 1) {
            codepoints_.clear();
        }
        codepoints_cnt_ = 0;
    }

    const Codepoint* Codepoints() const {
        MGO_ASSERT(codepoints_cnt_ != 0);
        if (codepoints_cnt_ == 1) {
            return &codepoint_;
        }
        return codepoints_.data();
    }

    size_t CodePointCount() const { return codepoints_cnt_; }

    int Width();

   private:
    std::vector<Codepoint> codepoints_;
    Codepoint codepoint_;
    int codepoints_cnt_ = 0;
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

// Use this if you really want performance, e.g. in a big loop.
__always_inline Result ThisCharacterInline(std::string_view str, int64_t offset,
                                           Character& character,
                                           int& byte_len) {
    MGO_ASSERT(static_cast<size_t>(offset) < str.size());

    int64_t cur_offset = offset;
    int64_t end_offset = str.size();

    // ascii happy path
    if ((cur_offset <= end_offset - 2 && str[cur_offset] >= 0 &&
         str[cur_offset + 1] >= 0) ||
        cur_offset == end_offset - 1) {
        character.Set(str[cur_offset]);
        byte_len = 1;
        return kOk;
    }

    character.Clear();
    Codepoint codepoint;
    Codepoint last_codepoint;
    utf8proc_int32_t state = 0;
    while (cur_offset < end_offset) {
        int byte_eat;
        Result res = Utf8ToUnicode(&str[cur_offset], -1, byte_eat, codepoint);
        MGO_ASSERT(kOk == res);
        if (character.CodePointCount() == 0) {
            character.Push(codepoint);
        } else {
            utf8proc_bool is_break = utf8proc_grapheme_break_stateful(
                last_codepoint, codepoint, &state);
            if (is_break) {
                break;
            }
            character.Push(codepoint);
        }
        last_codepoint = codepoint;
        cur_offset += byte_eat;
    }
    byte_len = cur_offset - offset;
    return kOk;
}

// Make sure that str[offset] must be a character beginnig byte.
// offset shouldn't <= 0.
// Current only return kOk
Result PrevCharacter(std::string_view str, int64_t offset, Character& character,
                     int& byte_len);

bool IsWordSeperator(char c);

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