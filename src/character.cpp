#include "character.h"

#include <inttypes.h>

#include "term.h"

namespace mango {

bool IsUtf8BeginByte(char b) {
    return (static_cast<std::byte>(b) >> 6) != static_cast<std::byte>(0b10);
}

Result NextCharacterInUtf8(const std::string& str, int64_t offset,
                           std::vector<uint32_t>& character, int& byte_len,
                           int& width) {
    character.resize(1);
    byte_len = Utf8ToUnicode(&str[offset], character[0]);
    if (byte_len < 0) {
        byte_len = -byte_len;
        character[0] = kReplacementChar;
        width = 1;
        MGO_LOG_INFO("Meet error character in buffer");
        return kOk;
    }
    width = Terminal::WCWidth(character[0]);
    if (width <= 0) {
        MGO_LOG_INFO(
            "Meet non-printable or wcwidth == 0 character \\u%04" PRIx32
            " in buffer, width = %d",
            character[0], width);
        width = 1;
        if (character[0] == kTabChar) {
            ;
        } else {
            character[0] = kReplacementChar;
        }
    }
    return kOk;
}

Result PrevCharacterInUtf8(const std::string& str, int64_t offset,
                           std::vector<uint32_t>& character, int& byte_len,
                           int& width) {
    size_t origin_offset = offset;
    character.resize(1);
    offset--;
    while (offset >= 0) {
        if (IsUtf8BeginByte(str[offset])) {
            byte_len = Utf8ToUnicode(&str[offset], character[0]);
            if (byte_len < 0) {
                byte_len = -byte_len;
                character[0] = kReplacementChar;
                width = 1;
                MGO_LOG_INFO("Meet error character in buffer");
                return kOk;
            }
            width = Terminal::WCWidth(character[0]);
            if (width <= 0) {
                MGO_LOG_INFO(
                    "Meet non-printable or wcwidth == 0 character \\u%04" PRIx32
                    " in buffer, width = %d",
                    character[0], width);
                width = 1;
                if (character[0] == kTabChar) {
                    ;
                } else {
                    character[0] = kReplacementChar;
                }
            }
            break;
        }
        offset--;
    }
    if (offset < 0) {
        byte_len = origin_offset;
        character[0] = kReplacementChar;
    }
    return kOk;
}

Result NextWord(const std::string& str, size_t offset,
                size_t& next_word_offset) {
    std::vector<uint32_t> character;
    int byte_len;
    int c_width;
    bool found_non_word_character = false;
    while (offset < str.size()) {
        Result res =
            NextCharacterInUtf8(str, offset, character, byte_len, c_width);
        MGO_ASSERT(res == kOk);
        if (character[0] == kUnderLine || isalnum(character[0])) {
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

Result PrevWord(const std::string& str, size_t offset,
                size_t& prev_word_offset) {
    if (offset == 0) {
        prev_word_offset = 0;
        return kOk;
    }

    int64_t inner_offset = offset;
    std::vector<uint32_t> character;
    int byte_len;
    int c_width;
    bool found_word_character = false;
    while (inner_offset > 0) {
        Result res = PrevCharacterInUtf8(str, inner_offset, character, byte_len,
                                         c_width);
        MGO_ASSERT(res == kOk);
        if (character[0] == kUnderLine || isalnum(character[0])) {
            found_word_character = true;
        } else {
            if (found_word_character) {
                prev_word_offset = inner_offset;
                return kOk;
            }
        }
        inner_offset -= byte_len;
    }
    prev_word_offset = 0;
    return kOk;
}

}  // namespace mango