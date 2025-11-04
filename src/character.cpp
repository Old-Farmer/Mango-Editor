#include "character.h"

#include <cassert>

#include "term.h"

namespace mango {

bool IsUtf8BeginByte(char b) {
    return (static_cast<std::byte>(b) >> 6) != static_cast<std::byte>(0b10);
}

Result NextCharacterInUtf8(const std::string& str, int offset,
                           std::vector<uint32_t>& character, int& byte_len,
                           int& width) {
    character.resize(1);
    byte_len = Utf8ToUnicode(&str[offset], character[0]);
    if (byte_len < 0) {
        byte_len = -byte_len;
        character[0] = kReplacementChar;
        assert(false);
    }
    width = Terminal::WCWidth(character[0]);
    // non-printable
    if (width == -1) {
        if (character[0] == kTabChar) {
            return kOk;
        }
        character[0] = kReplacementChar;
        width = 1;
        assert(((void)"width == -1", false));
    } else if (width == 0) {
        character[0] = kReplacementChar;
        width = 1;
        assert(((void)"width == 0", false));
    }
    return kOk;
}

Result PrevCharacterInUtf8(const std::string& str, int offset,
                           std::vector<uint32_t>& character, int& byte_len,
                           int& width) {
    character.resize(1);
    offset--;
    while (offset >= 0) {
        if (IsUtf8BeginByte(str[offset])) {
            byte_len = Utf8ToUnicode(&str[offset], character[0]);
            if (byte_len < 0) {
                byte_len = -byte_len;
                character[0] = kReplacementChar;
                assert(false);
            }
            width = Terminal::WCWidth(character[0]);
            if (width == -1) {
                if (character[0] == kTabChar) {
                    return kOk;
                }
                character[0] = kReplacementChar;
                width = 1;
                assert(((void)"width == -1", false));
            } else if (width == 0) {
                character[0] = kReplacementChar;
                width = 1;
                assert(((void)"width == 0", false));
            }
            break;
        }
        offset--;
    }
    return kOk;
}

}  // namespace mango