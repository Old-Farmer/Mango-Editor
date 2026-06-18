#pragma once
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "result.h"
#include "termbox2.h"
#include "text_tree.h"
#include "utf8.h"
#include "utils.h"

namespace charxed {

constexpr Codepoint kSpaceChar = ' ';           // Use this to avoid confusion
constexpr Codepoint kReplacementChar = 0xFFFD;  // �
constexpr int kReplacementCharWidth = 1;

constexpr const char* kSpace = " ";
constexpr const char* kReplacement = "�";

constexpr Codepoint kTrailingSpace = 0x00B7;  // "·"
constexpr Codepoint kTrailingTab = 0x2192;    // "→"

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
        codepoints_cnt_++;
        if (codepoints_cnt_ == 1) {
            codepoint_ = codepoint;
        } else if (codepoints_cnt_ == 2) {
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
        CHX_ASSERT(codepoints_cnt_ != 0);
        if (codepoints_cnt_ == 1) {
            return &codepoint_;
        }
        return codepoints_.data();
    }

    size_t CodePointCount() const { return codepoints_cnt_; }

    int Width() const;

    // Useful when logging
    std::string ToString() const;

    bool operator==(const Character& other) const {
        if (CodePointCount() != other.CodePointCount()) {
            return false;
        }
        size_t sz = CodePointCount();
        for (size_t i = 0; i < sz; i++) {
            if (Codepoints()[i] != other.Codepoints()[i]) {
                return false;
            }
        }
        return true;
    }

   private:
    std::vector<Codepoint> codepoints_;
    // single codepoint grapheme optimization(most case)
    Codepoint codepoint_;
    int codepoints_cnt_ = 0;
};

inline bool IsAscii(char c) { return (c & 0b10000000) == 0; }

int CharacterWidth(const Codepoint* codepoints, size_t cnt);

// We assume that
// 1. the str is pure utf-8 format, no error coding, because we check errors for
// all outer source.
// 2. no "\r\n", so two adjacent ascii can't be one grapheme; If "\r\n"
// accidentally exists, we also treat them as two graphemes.
//
// TODO:
// 1. make it iterator

// str[offset] must be a character beginnig byte, otherwise behavior is
// undefined.
// offset shouldn't >= str.size().
// Current only return kOk.
Result ThisCharacterInner(std::string_view str, int64_t offset,
                          Character& character, int& byte_len);

// Wrap of ThisCharacterInner, and ascii friendly
inline Result ThisCharacter(std::string_view str, int64_t offset,
                            Character& character, int& byte_len) {
    CHX_ASSERT(static_cast<size_t>(offset) < str.size());
    int64_t cur_offset = offset;
    int64_t end_offset = str.size();
    // ascii happy path
    if ((cur_offset <= end_offset - 2 && IsAscii(str[cur_offset]) &&
         IsAscii(str[cur_offset + 1])) ||
        (cur_offset == end_offset - 1)) {
        character.Set(str[cur_offset]);
        byte_len = 1;
        return kOk;
    }
    return ThisCharacterInner(str, offset, character, byte_len);
}

// iter must be a character beginning pos, otherwise behavior is
// undefined.
// iter != end.
// return a iter at the next character beginning pos, character will be set to
// the current character.
TextTree::Iterator NextCharacterInner(TextTree::Iterator iter,
                                      TextTree::Iterator end,
                                      Character& character);

// A ascii friendly wrapping of NextCharacterInner
inline TextTree::Iterator NextCharacter(TextTree::Iterator iter,
                                        TextTree::Iterator end,
                                        Character& character) {
    CHX_ASSERT(iter != end);
    char ascii_c = iter.ThisByte();
    if (IsAscii(ascii_c)) {
        iter.NextByte();
        if (iter == end || IsAscii(iter.ThisByte())) {
            character.Set(ascii_c);
            return iter;
        }
        iter.PrevByte();
    }
    return NextCharacterInner(iter, end, character);
}

// Make sure that str[offset] must be a character beginnig byte.
// offset shouldn't <= 0.
// Current only return kOk
Result PrevCharacterInner(std::string_view str, int64_t offset,
                          Character& character, int& byte_len);

// Wrap of PrevCharacterInner, and ascii friendly
inline Result PrevCharacter(std::string_view str, int64_t offset,
                            Character& character, int& byte_len) {
    CHX_ASSERT(offset > 0);
    // ascii happy path
    if ((offset > 1 && IsAscii(str[offset - 2]) && IsAscii(str[offset + 1])) ||
        offset == 1) {
        character.Set(str[offset - 1]);
        byte_len = 1;
        return kOk;
    }
    return PrevCharacterInner(str, offset, character, byte_len);
}

// Make sure that iter must be a character beginnig pos.
// iter shouldn't == begin.
// return a iter at the prev character begin pos, character will be set to the
// prev character.
TextTree::Iterator PrevCharacterInner(TextTree::Iterator iter,
                                      TextTree::Iterator begin,
                                      Character& character);

// A ascii friendly wrapping of NextCharacterInner
inline TextTree::Iterator PrevCharacter(TextTree::Iterator iter,
                                        TextTree::Iterator begin,
                                        Character& character) {
    CHX_ASSERT(iter != begin);
    iter.PrevByte();
    char ascii_c = iter.ThisByte();
    if (IsAscii(ascii_c)) {
        if (iter == begin) {
            character.Set(ascii_c);
            return iter;
        }
        iter.PrevByte();
        if (IsAscii(iter.ThisByte())) {
            character.Set(ascii_c);
            iter.NextByte();
            return iter;
        }
        iter.NextByte();
    }
    iter.NextByte();
    return PrevCharacterInner(iter, begin, character);
}

// Check whether between byte_offset - 1 and byte_offset is a valid character
// boundry. Only context is that byte_offset is a codepoint start.
inline bool CharacterBoundaryValid(std::string_view str, size_t byte_offset) {
    if (byte_offset == str.size() || byte_offset == 0) {
        return true;
    }
    // ascii happy path
    if (IsAscii(str[byte_offset - 1]) && IsAscii(str[byte_offset])) {
        return true;
    }

    int byte_len;
    Codepoint cp;
    Utf8ToUnicode(str.data() + byte_offset, str.size() - byte_offset, byte_len,
                  cp);
    Character c;
    int new_byte_len;
    PrevCharacterInner(str, byte_offset + byte_len, c, new_byte_len);
    return new_byte_len == byte_len;
}

bool IsWordSeperator(char c);

// return a iter at the next word begin
TextTree::Iterator NextWordBegin(TextTree::Iterator iter,
                                 TextTree::Iterator end);

// return a iter at the this/next word end.
TextTree::Iterator NextWordEnd(TextTree::Iterator iter, TextTree::Iterator end);

// return iter at the this/prev word end
TextTree::Iterator PrevWordBegin(TextTree::Iterator iter,
                                 TextTree::Iterator begin);

// return the word [begin, end)
// return size 0 view if iter is blank
TextTree::TextView ThisWord(TextTree::Iterator iter, TextTree::TextView line);

// Find target character, exclude current pos
// return target iter if found,
// else return null.
std::optional<TextTree::Iterator> PrevSpecificCharacter(
    TextTree::Iterator iter, const Character& target, TextTree::Iterator begin);
std::optional<TextTree::Iterator> NextSpecificCharacter(TextTree::Iterator iter,
                                                        const Character& target,
                                                        TextTree::Iterator end);

inline std::vector<char> PairOpens() {
    return {'(', '[', '{', '\"', '\'', '`'};
}

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
        case '`':
            return {true, c};
    }
    return {false, -1};
}

// unused now
inline std::pair<bool, char> IsPairClose(char c) {
    switch (c) {
        case '}':
            return {true, '{'};
        case ']':
            return {true, '['};
        case ')':
            return {true, '('};
        case '\'':
        case '\"':
        case '`':
            return {true, c};
    }
    return {false, -1};
}

// Can open and close pair together ?
inline bool IsPair(char open, char close) {
    auto [is_pair, real_close] = IsPairOpen(open);
    return is_pair && real_close == close;
}

// If c is the open or close part of a bracket.
inline bool IsBracket(char c) {
    return c == '(' || c == '{' || c == '[' || c == ')' || c == '}' || c == ']';
}

// Currently, only single line quotes.
// TODO: multi-line quotes?
inline bool isQuote(char c) { return c == '\"' || c == '\'' || c == '`'; }

// If c is the open or close part of a pair.
inline bool IsPair(char c) { return IsBracket(c) || isQuote(c); }

// Find closest bracket character, include current pos.
// iter should be in a bracket.
// Requirement: iter != range.end
// return range.end if can't be found
// else retrun the iter where the bracket open/close is, and a bool indicate
// whether is open or close.
std::tuple<TextTree::Iterator, bool> ClosestBracket(
    TextTree::Iterator iter, const TextTree::TextView& range);

// Find a pair of bracket around iter.
// open should be real bracket open.
// Requirement: iter != range.end.
// return TextView of the bracket range if found,
// else return Size() == 0 TextView
TextTree::TextView FindBracketPairAround(TextTree::Iterator iter,
                                         const TextTree::TextView& range,
                                         char open);

// Find a pair of quote around iter in a line.
// return TextView of the quote range if found,
// else return Size() == 0 TextView
TextTree::TextView FindQuotePairAroundInLine(TextTree::Iterator iter,
                                             const TextTree::TextView& line,
                                             char quote);

size_t StringWidth(const std::string& str);

}  // namespace charxed
