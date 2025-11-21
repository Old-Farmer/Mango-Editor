#include "character.h"

namespace mango {

int Character::Width() {
    MGO_ASSERT(!codepoints_.empty());
    return CharacterWidth(codepoints_.data(), codepoints_.size());
}

std::string Character::ToString() {
    std::string str;
    char buf[4];
    for (Codepoint codepoint : codepoints_) {
        int len = UnicodeToUtf8(codepoint, buf);
        if (len == 0) {
            return "";
        } else {
            str.append(buf, len);
        }
    }
    return str;
}

// TODO: I haven't decide a totally right way to calc character width,
// use the following appraoch as a tmp workaround.
// Possible ref:
// https://github.com/jameslanska/unicode-display-width
// https://github.com/helix-editor/helix/issues/6012
// https://github.com/kovidgoyal/kitty/issues/5047
// https://mitchellh.com/writing/grapheme-clusters-in-terminals
// https://hexdocs.pm/string_width/internals.html#:~:text=The%20width%20of%20a%20grapheme,codepoint%2C%20its%20width%20is%20wide.

// Finally ref:
// https://sw.kovidgoyal.net/kitty/text-sizing-protocol

static constexpr Codepoint kRIStart = 0x1F1E6;
static constexpr int kRICnt = 26;
static constexpr Codepoint kVS15 = 0xFE0E;
static constexpr Codepoint kVS16 = 0xFE0F;

static int WCWidth(Codepoint codepoint) {
    // Fix RI width
    if (codepoint >= kRIStart && codepoint < kRIStart + kRICnt) {
        return 2;
    }
    return utf8proc_charwidth(codepoint);
}
// We use the first codepoint wcwidth. If we encounter VS later, we adjust the
// width. It is pretty fine enough, although VSes only take effect after certain
// categories of codepoints, but we don't check categories now.
int CharacterWidth(const Codepoint* codepoints, size_t cnt) {
    int width = WCWidth(codepoints[0]);
    if (cnt == 1) {
        return width;
    }

    for (size_t i = 1; i < cnt; i++) {
        if (codepoints[i] == kVS16 && width == 1) {
            width = 2;
        } else if (codepoints[i] == kVS15 && width == 2) {
            width = 1;
        }
    }
    return width;
}

bool IsUtf8BeginByte(char b) {
    return (static_cast<std::byte>(b) >> 6) != static_cast<std::byte>(0b10);
}

bool CheckUtf8Valid(std::string_view str) {
    size_t offset = 0;
    size_t end_offset = str.size();
    Codepoint codepoint;
    while (offset < end_offset) {
        int byte_len;
        if (kInvalidCoding == Utf8ToUnicode(&str[offset], end_offset - offset,
                                            byte_len, codepoint)) {
            return false;
        }
        offset += byte_len;
    }
    return true;
}

Result ThisCharacterInUtf8(std::string_view str, int64_t offset,
                           Character& character, int& byte_len) {
    MGO_ASSERT(static_cast<size_t>(offset) < str.size());

    character.Clear();
    Codepoint codepoint;
    Codepoint last_codepoint;
    utf8proc_int32_t state = 0;
    int64_t cur_offset = offset;
    int64_t end_offset = str.size();
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

Result PrevCharacterInUtf8(std::string_view str, int64_t offset,
                           Character& character, int& byte_len) {
    MGO_ASSERT(offset > 0);

    character.Clear();
    int64_t cur_offset = offset;
    cur_offset--;
    int byte_eat;
    std::vector<Codepoint> codepoints_reverse;
    std::vector<int64_t> offset_reverse;
    utf8proc_int32_t state;
    while (cur_offset >= 0) {
        if (IsUtf8BeginByte(str[cur_offset])) {
            Codepoint codepoint;
            Result res =
                Utf8ToUnicode(&str[cur_offset], -1, byte_eat, codepoint);
            MGO_ASSERT(res == kOk);
            if (!codepoints_reverse.empty()) {
                // Safe as a state 0 grapheme break check.
                // See
                // https://github.com/JuliaStrings/utf8proc/discussions/314#discussioncomment-14983853
                // A lot of codepoint is other bound class, including ascii
                // and chinese, so it will be likey to stop early.
                if (utf8proc_get_property(codepoint)->boundclass ==
                    UTF8PROC_BOUNDCLASS_OTHER) {
                    state = 0;
                    if (utf8proc_grapheme_break_stateful(
                            codepoint, codepoints_reverse.back(), &state)) {
                        break;
                    }
                }
            }
            codepoints_reverse.push_back(codepoint);
            offset_reverse.push_back(cur_offset);
        }
        cur_offset--;
    }

    // Impossible becasue we assume coding is correct.
    MGO_ASSERT(!codepoints_reverse.empty());

    // We have found a break or we touch the beginning of the str,
    // we start find the last break before offset starting from the
    // codepoint_reverse.back().

    // Only one codepoint, fast return, most case.
    if (codepoints_reverse.size() == 1) {
        character.Push(codepoints_reverse.back());
        byte_len = offset - offset_reverse.back();
        return kOk;
    }

    state = 0;
    int last_break = -1;
    for (int i = codepoints_reverse.size() - 2; i >= 0; i--) {
        if (utf8proc_grapheme_break_stateful(codepoints_reverse[i + 1],
                                             codepoints_reverse[i], &state)) {
            last_break = i;
            state = 0;
        }
    }
    if (last_break == -1) {
        last_break = codepoints_reverse.size() - 1;
    }
    for (int i = last_break; i >= 0; i--) {
        character.Push(codepoints_reverse[i]);
    }
    byte_len = offset - offset_reverse[last_break];
    return kOk;
}

bool IsWordCharacter(char c) { return c == '_' || isalnum(c); }

Result NextWordBegin(const std::string& str, size_t offset,
                     size_t& next_word_offset) {
    Character character;
    int byte_len;
    bool found_non_word_character = false;
    while (offset < str.size()) {
        Result res = ThisCharacterInUtf8(str, offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordCharacter(c)) {
            if (found_non_word_character) {
                next_word_offset = offset;
                return kOk;
            }
        } else {
            found_non_word_character = true;
        }
        offset += byte_len;
    }
    next_word_offset = str.size();
    return kOk;
}

Result WordEnd(const std::string& str, size_t offset, bool one_more_character,
               size_t& next_word_end_offset) {
    if (offset == str.size()) {
        return kOk;
    }

    Character character;
    int byte_len;
    bool found_word_character = false;
    Result res = ThisCharacterInUtf8(str, offset, character, byte_len);
    MGO_ASSERT(res == kOk);
    offset += byte_len;
    while (offset < str.size()) {
        Result res = ThisCharacterInUtf8(str, offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordCharacter(c)) {
            found_word_character = true;
        } else {
            if (found_word_character) {
                if (one_more_character) {
                    next_word_end_offset = offset;
                } else {
                    next_word_end_offset = offset - 1;  // 1 is safe for ascii
                }
                return kOk;
            }
        }
        offset += byte_len;
    }
    if (found_word_character) {
        if (one_more_character) {
            next_word_end_offset = str.size();
        } else {
            next_word_end_offset = str.size() - 1;
        }
    } else {
        next_word_end_offset = str.size();
    }
    return kOk;
}

Result WordBegin(const std::string& str, size_t offset,
                 size_t& prev_word_offset) {
    if (offset == 0) {
        prev_word_offset = 0;
        return kOk;
    }

    int64_t inner_offset = offset;
    Character character;
    int byte_len;
    bool found_word_character = false;
    while (inner_offset > 0) {
        Result res =
            PrevCharacterInUtf8(str, inner_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordCharacter(c)) {
            found_word_character = true;
        } else {
            if (found_word_character) {
                prev_word_offset = inner_offset;
                return kOk;
            }
        }
        inner_offset -= byte_len;
    }
    if (found_word_character) {
        prev_word_offset = 0;
        return kOk;
    } else {
        prev_word_offset = 0;
        return kNotExist;
    }
    return kOk;
}

size_t StringWidth(const std::string& str) {
    Character character;
    size_t offset = 0;
    size_t width = 0;
    while (offset < str.size()) {
        int len;
        Result res = ThisCharacterInUtf8(str, offset, character, len);
        MGO_ASSERT(res == kOk);
        int character_width = character.Width();
        if (character_width <= 0) {
            character_width = kReplacementCharWidth;
        }
        offset += len;
        width += character_width;
    }
    return width;
}

}  // namespace mango