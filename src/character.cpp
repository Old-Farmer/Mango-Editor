#include "character.h"

namespace charxed {

int Character::Width() const {
    CHX_ASSERT(codepoints_cnt_ != 0);
    return CharacterWidth(Codepoints(), CodePointCount());
}

std::string Character::ToString() const {
    std::string str;
    char c[kMaxBytesUtf8Codepoint];
    for (size_t i = 0; i < CodePointCount(); i++) {
        int len = UnicodeToUtf8(Codepoints()[i], c);
        CHX_ASSERT(len > 0);
        str.append(c, len);
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

static constexpr bool kIsWordSeparator[128] = {
    // 0x00-0x1F: control codes
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // 0x00-0x07
    false,
    false,
    true,  // 0xa '\n'
    false,
    false,
    false,
    false,
    false,  // 0x08-0x0F
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // 0x10-0x17
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // 0x18-0x1F

    // 0x20-0x2F
    true,   // 0x20 ' ' (space)
    true,   // 0x21 '!'
    true,   // 0x22 '"'
    true,   // 0x23 '#'
    false,  // 0x24 '$'
    true,   // 0x25 '%'
    true,   // 0x26 '&'
    true,   // 0x27 '\''
    true,   // 0x28 '('
    true,   // 0x29 ')'
    true,   // 0x2A '*'
    true,   // 0x2B '+'
    true,   // 0x2C ','
    true,   // 0x2D '-'
    true,   // 0x2E '.'
    true,   // 0x2F '/'

    // 0x30-0x3F: number 0-9 and more signs
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // 0-7
    false,
    false,  // 8-9
    true,   // 0x3A ':'
    true,   // 0x3B ';'
    true,   // 0x3C '<'
    true,   // 0x3D '='
    true,   // 0x3E '>'
    true,   // 0x3F '?'

    // 0x40-0x4F: @, A-O
    true,  // 0x40 '@'
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // A-H
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // I-O

    // 0x50-0x5F: P-Z, [, \, ], ^, _
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // P-W
    false,
    false,
    false,  // X-Z
    true,   // 0x5B '['
    true,   // 0x5C '\\'
    true,   // 0x5D ']'
    true,   // 0x5E '^'
    false,  // 0x5F '_'

    // 0x60-0x6F: `, a-o
    true,  // 0x60 '`'
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // a-h
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // i-o

    // 0x70-0x7F: p-z, {, |, }, ~, DEL
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,  // p-w
    false,
    false,
    false,  // x-z
    true,   // 0x7B '{'
    true,   // 0x7C '|'
    true,   // 0x7D '}'
    true,   // 0x7E '~'
    false,  // 0x7F DEL
};

bool IsWordSeperator(char c) { return kIsWordSeparator[static_cast<int>(c)]; }

Result ThisCharacterInner(std::string_view str, int64_t offset,
                          Character& character, int& byte_len) {
    CHX_ASSERT(static_cast<size_t>(offset) < str.size());
    int64_t cur_offset = offset;
    int64_t end_offset = str.size();

    character.Clear();
    Codepoint codepoint;
    Codepoint last_codepoint;
    utf8proc_int32_t state = 0;
    while (cur_offset < end_offset) {
        int byte_eat;
        Utf8ToUnicode(&str[cur_offset], -1, byte_eat, codepoint);
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

TextTree::Iterator NextCharacterInner(TextTree::Iterator iter,
                                      TextTree::Iterator end,
                                      Character& character) {
    character.Clear();
    Codepoint codepoint;
    Codepoint last_codepoint;
    utf8proc_int32_t state = 0;
    while (iter != end) {
        auto next = iter.NextCodepoint(codepoint);
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
        iter = next;
    }
    return iter;
}

Result PrevCharacterInner(std::string_view str, int64_t offset,
                          Character& character, int& byte_len) {
    CHX_ASSERT(offset > 0);

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
            Utf8ToUnicode(&str[cur_offset], -1, byte_eat, codepoint);
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
    CHX_ASSERT(!codepoints_reverse.empty());

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

TextTree::Iterator PrevCharacterInner(TextTree::Iterator iter,
                                      TextTree::Iterator begin,
                                      Character& character) {
    CHX_ASSERT(iter != begin);

    character.Clear();
    std::vector<Codepoint> codepoints_reverse;
    std::vector<TextTree::Iterator> iter_reverse;
    utf8proc_int32_t state;
    Codepoint cp;
    while (iter != begin) {
        iter = iter.PrevCodepoint(cp);
        if (!codepoints_reverse.empty()) {
            // Safe as a state 0 grapheme break check.
            // See
            // https://github.com/JuliaStrings/utf8proc/discussions/314#discussioncomment-14983853
            // A lot of codepoint is other bound class, including ascii
            // and chinese, so it will be likey to stop early.
            if (utf8proc_get_property(cp)->boundclass ==
                UTF8PROC_BOUNDCLASS_OTHER) {
                state = 0;
                if (utf8proc_grapheme_break_stateful(
                        cp, codepoints_reverse.back(), &state)) {
                    break;
                }
            }
        }
        codepoints_reverse.push_back(cp);
        iter_reverse.push_back(iter);
    }

    // Impossible becasue we assume coding is correct.
    CHX_ASSERT(!codepoints_reverse.empty());

    // We have found a break or we touch the beginning of the str,
    // we start find the last break before offset starting from the
    // codepoint_reverse.back().

    // Only one codepoint, fast return, most case.
    if (codepoints_reverse.size() == 1) {
        character.Push(codepoints_reverse.back());
        return iter_reverse.back();
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
    return iter_reverse[last_break];
}

namespace {
constexpr int kCTypeWord = 1 << 0;
constexpr int kCTypeNonWordNonBlank = 1 << 1;
constexpr int kCTypeNonWordBlank = 1 << 2;

inline int GetCharacterType(const Character& c) {
    char ascii_c;
    if (c.Ascii(ascii_c) && IsWordSeperator(ascii_c)) {
        return ascii_c == kSpaceChar || ascii_c == '\t' || ascii_c == '\n'
                   ? kCTypeNonWordBlank
                   : kCTypeNonWordNonBlank;
    } else {
        return kCTypeWord;
    }
}

// return this, next, type
// if this == end, next will be end to and type will be unknown
inline std::tuple<TextTree::Iterator, TextTree::Iterator, int>
FindNextTargetType(TextTree::Iterator iter, TextTree::Iterator end,
                   int target) {
    Character c;
    int type;
    while (iter != end) {
        auto next = NextCharacter(iter, end, c);
        type = GetCharacterType(c);
        if (type & target) {
            return {iter, next, type};
        }
        iter = next;
    }
    return {iter, iter, type};
}

// return next, this and type
// return this == begin, then prev = begin and type will be unknown
inline std::tuple<TextTree::Iterator, TextTree::Iterator, int>
FindPrevTargetType(TextTree::Iterator iter, TextTree::Iterator begin,
                   int target) {
    Character c;
    int type;
    while (iter != begin) {
        auto prev = PrevCharacter(iter, begin, c);
        type = GetCharacterType(c);
        if (type & target) {
            return {iter, prev, type};
        }
        iter = prev;
    }
    return {iter, iter, type};
}

}  // namespace

// cur is word, find next non-blank non-word, or find next blank non-word, then
// find next non-blank non-word, or word;
// cur is non-word and blank, find next
// word or non-blank non-word; cur is non-word non-blank, find next word or
// blank non-word, if is word done, else find next word or non-blank non-word
TextTree::Iterator NextWordBegin(TextTree::Iterator iter,
                                 TextTree::Iterator end) {
    if (iter == end) {
        return iter;
    }

    Character c;
    auto next = NextCharacter(iter, end, c);
    int type;
    switch (GetCharacterType(c)) {
        case kCTypeWord:
            std::tie(iter, next, type) = FindNextTargetType(
                next, end, kCTypeNonWordNonBlank | kCTypeNonWordBlank);
            if (iter == end || type == kCTypeNonWordNonBlank) {
                return iter;
            }
            return std::get<0>(FindNextTargetType(
                next, end, kCTypeNonWordNonBlank | kCTypeWord));
        case kCTypeNonWordNonBlank: {
            std::tie(iter, next, type) =
                FindNextTargetType(next, end, kCTypeWord | kCTypeNonWordBlank);
            if (iter == end || type == kCTypeWord) {
                return iter;
            }
            return std::get<0>(FindNextTargetType(
                next, end, kCTypeWord | kCTypeNonWordNonBlank));
        }
        case kCTypeNonWordBlank:
            return std::get<0>(FindNextTargetType(
                next, end, kCTypeWord | kCTypeNonWordNonBlank));
    }
    return iter;  // make compiler happy
}

// cur is word, find next non-blank non-word or blank non-word;
// cur is non-word and blank, first find next word, then find non-word non-blank
// or non-word blank or first find next non-blank non-word, then find next word
// or non-word non-blank;
// cur is non-word non-blank, find next word or blank non-word
TextTree::Iterator NextWordEnd(TextTree::Iterator iter,
                               TextTree::Iterator end) {
    if (iter == end) {
        return iter;
    }

    Character c;
    auto next = NextCharacter(iter, end, c);
    int type;
    switch (GetCharacterType(c)) {
        case kCTypeWord:
            return std::get<0>(FindNextTargetType(
                next, end, kCTypeNonWordNonBlank | kCTypeNonWordBlank));
        case kCTypeNonWordNonBlank:
            return std::get<0>(
                FindNextTargetType(next, end, kCTypeWord | kCTypeNonWordBlank));
        case kCTypeNonWordBlank:
            std::tie(iter, next, type) = FindNextTargetType(
                next, end, kCTypeWord | kCTypeNonWordNonBlank);
            if (iter == end) {
                return iter;
            }
            if (type == kCTypeWord) {
                return std::get<0>(FindNextTargetType(
                    next, end, kCTypeNonWordNonBlank | kCTypeNonWordBlank));
            } else if (type == kCTypeNonWordNonBlank) {
                return std::get<0>(FindNextTargetType(
                    next, end, kCTypeWord | kCTypeNonWordBlank));
            } else {
                CHX_ASSERT(false);
            }
    }
    return iter;  // make compiler happy
}

// prev is word, find prev non-blank non-word or blank non-word;
// prev is non-word and blank, first find prev word, then find non-word
// non-blank or non-word blank or first prev next non-blank non-word, then find
// prev word or non-word non-blank;
// prev is non-word non-blank, find prev word or blank non-word
TextTree::Iterator PrevWordBegin(TextTree::Iterator iter,
                                 TextTree::Iterator begin) {
    if (iter == begin) {
        return iter;
    }

    Character c;
    auto prev = PrevCharacter(iter, begin, c);
    int type;
    switch (GetCharacterType(c)) {
        case kCTypeWord:
            return std::get<0>(FindPrevTargetType(
                prev, begin, kCTypeNonWordNonBlank | kCTypeNonWordBlank));
        case kCTypeNonWordNonBlank:
            return std::get<0>(FindPrevTargetType(
                prev, begin, kCTypeWord | kCTypeNonWordBlank));
        case kCTypeNonWordBlank:
            std::tie(iter, prev, type) = FindPrevTargetType(
                prev, begin, kCTypeWord | kCTypeNonWordNonBlank);
            if (iter == begin) {
                return iter;
            }
            if (type == kCTypeWord) {
                return std::get<0>(FindPrevTargetType(
                    prev, begin, kCTypeNonWordNonBlank | kCTypeNonWordBlank));
            } else if (type == kCTypeNonWordNonBlank) {
                return std::get<0>(FindPrevTargetType(
                    prev, begin, kCTypeWord | kCTypeNonWordBlank));
            }
    }
    return iter;  // make compiler happy
}

TextTree::TextView ThisWord(TextTree::Iterator iter, TextTree::TextView line) {
    if (iter == line.end) {
        return {iter, iter};
    }

    Character c;
    NextCharacter(iter, line.end, c);
    int target_t;
    switch (GetCharacterType(c)) {
        case kCTypeWord:
            target_t = kCTypeNonWordNonBlank | kCTypeNonWordBlank;
            break;
        case kCTypeNonWordNonBlank:
            target_t = kCTypeWord | kCTypeNonWordBlank;
            break;
        case kCTypeNonWordBlank:
            return {iter, iter};
    }
    TextTree::TextView res;
    std::tie(res.begin, std::ignore, std::ignore) =
        FindPrevTargetType(iter, line.begin, target_t);
    std::tie(res.end, std::ignore, std::ignore) =
        FindNextTargetType(iter, line.begin, target_t);
    return res;
}

std::optional<TextTree::Iterator> PrevSpecificCharacter(
    TextTree::Iterator iter, const Character& target,
    TextTree::Iterator begin) {
    Character c;
    while (iter != begin) {
        iter = PrevCharacter(iter, begin, c);
        if (c == target) {
            return iter;
        }
    }
    return {};
}

std::optional<TextTree::Iterator> NextSpecificCharacter(
    TextTree::Iterator iter, const Character& target, TextTree::Iterator end) {
    Character c;
    if (iter != end) iter = NextCharacter(iter, end, c);
    while (iter != end) {
        auto next = NextCharacter(iter, end, c);
        if (c == target) {
            return iter;
        }
        iter = next;
    }
    return {};
}

// Find first open bracket before iter or first close bracket after iter.
// Also consider the current pos.
std::tuple<TextTree::Iterator, bool> ClosestBracket(
    TextTree::Iterator iter, const TextTree::TextView& range) {
    Character c;
    auto iter_after = NextCharacter(iter, range.end, c);
    char ascii_c;
    if (c.Ascii(ascii_c) && IsBracket(ascii_c)) {
        return {iter, IsPairOpen(ascii_c).first};
    }
    auto iter_before = iter;
    while (iter_before != range.begin && iter_after != range.end) {
        auto next = NextCharacter(iter_after, range.end, c);
        if (c.Ascii(ascii_c) && IsBracket(ascii_c) &&
            !IsPairOpen(ascii_c).first) {
            return {iter_after, false};
        }
        auto prev = PrevCharacter(iter_before, range.begin, c);
        if (c.Ascii(ascii_c) && IsBracket(ascii_c) &&
            IsPairOpen(ascii_c).first) {
            return {prev, true};
        }
        iter_after = next;
        iter_before = prev;
    }
    return {range.end, true};
}

TextTree::TextView FindBracketPairAround(TextTree::Iterator iter,
                                         const TextTree::TextView& range,
                                         char open) {
    auto [is_pair, close] = IsPairOpen(open);
    (void)is_pair;
    CHX_ASSERT(is_pair);
    TextTree::TextView res;
    // Check current pos first
    bool open_found = false;
    bool close_found = false;
    Character c;
    auto iter_after = NextCharacter(iter, range.end, c);
    char ascii_c;
    if (c.Ascii(ascii_c)) {
        if (ascii_c == open) {
            open_found = true;
            res.begin = iter;
        } else if (ascii_c == close) {
            close_found = true;
            res.end = iter_after;
        }
    }
    if (!open_found) {
        // We should skip paired brackets, so we use a cnt to track that
        // unpaired open count like a stack.
        // the same as close finding code below.
        int64_t unpaired_open_cnt = 0;
        while (iter != range.begin) {
            iter = PrevCharacter(iter, range.begin, c);
            if (c.Ascii(ascii_c)) {
                if (ascii_c == open) {
                    unpaired_open_cnt++;
                    if (unpaired_open_cnt == 1) {
                        open_found = true;
                        res.begin = iter;
                        break;
                    }
                } else if (ascii_c == close) {
                    unpaired_open_cnt--;
                }
            }
        }
    }
    if (!close_found) {
        int64_t unpaired_close_cnt = 0;
        iter = iter_after;
        while (iter != range.end) {
            iter = NextCharacter(iter, range.end, c);
            char ascii_c;
            if (c.Ascii(ascii_c)) {
                if (ascii_c == close) {
                    unpaired_close_cnt++;
                    if (unpaired_close_cnt == 1) {
                        close_found = true;
                        res.end = iter;
                        break;
                    }
                } else if (ascii_c == open) {
                    unpaired_close_cnt--;
                }
            }
        }
    }
    if (open_found && close_found) {
        return res;
    }
    return {range.end, range.end};
}

TextTree::TextView FindQuotePairAroundInLine(TextTree::Iterator iter,
                                             const TextTree::TextView& line,
                                             char quote) {
    TextTree::TextView res = {line.end, {}};
    TextTree::Iterator cur_iter = line.begin;
    for (TextTree::Iterator next; cur_iter != line.end; cur_iter = next) {
        // We havn't find any quote in [line.begin, iter], meaning no quote.
        if (iter < cur_iter && res.begin == line.end) {
            break;
        }

        Character c;
        next = NextCharacter(cur_iter, line.end, c);
        char ascii_c;
        if (!c.Ascii(ascii_c) || ascii_c != quote) {
            continue;
        }
        bool real_quote = true;
        // if quote " or ' is Followed by a \, it is not a quote.
        if ((quote == '\"' || quote == '\'') && cur_iter != line.begin) {
            PrevCharacter(cur_iter, line.begin, c);
            if (c.Ascii(ascii_c) && ascii_c == '\\') {
                real_quote = false;
            }
        }
        if (!real_quote) {
            continue;
        }
        if (cur_iter < iter) {
            res.begin = cur_iter;
        } else if (cur_iter == iter && res.begin == line.end) {
            res.begin = cur_iter;
        } else {
            res.end = next;
            break;
        }
    }
    if (res.begin != line.end && cur_iter != line.end) {
        return res;
    }
    return {line.end, line.end};
}

size_t StringWidth(const std::string& str) {
    Character character;
    size_t offset = 0;
    size_t width = 0;
    while (offset < str.size()) {
        int len;
        ThisCharacter(str, offset, character, len);
        int character_width = character.Width();
        if (character_width <= 0) {
            character_width = kReplacementCharWidth;
        }
        offset += len;
        width += character_width;
    }
    return width;
}

}  // namespace charxed
