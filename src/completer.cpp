#include "completer.h"

#include "buffer.h"
#include "character.h"
#include "cursor.h"
#include "window.h"

namespace mango {

BufferBasicWordCompleter::BufferBasicWordCompleter(const Buffer* buffer) {
    buffer_ = buffer;
}

void BufferBasicWordCompleter::Suggest(const Pos& cursor_pos,
                                       std::vector<std::string>& menu_entries) {
    std::string word;
    int64_t byte_offset = cursor_pos.byte_offset;
    const std::string& cur_line = buffer_->GetLine(cursor_pos.line);
    Character character;
    int byte_len;
    while (byte_offset >= 0) {
        Result res =
            PrevCharacterInUtf8(cur_line, byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordCharacter(c)) {
            word.push_back(c);
        } else {
            break;
        }
        byte_offset -= byte_len;
    }
    suggestions_ = words_trie_.PrefixWith(word);
    for (size_t i = 0; i < suggestions_.size(); i++) {
        // erase self
        if (suggestions_[i].size() == word.size()) {
            suggestions_.erase(suggestions_.begin() + i);
            break;
        }
    }
    menu_entries = suggestions_;
    bytes_of_word_before_cursor_ = word.size();
}

void BufferBasicWordCompleter::Accept(size_t index, Cursor* cursor) {
    MGO_ASSERT(cursor->in_window);
    MGO_ASSERT(index < suggestions_.size());
    Pos cursor_pos = cursor->ToPos();
    cursor->in_window->Replace(
        {{cursor_pos.line,
          cursor_pos.byte_offset - bytes_of_word_before_cursor_},
         cursor_pos},
        std::move(suggestions_[index]));  // Ignore ret
    suggestions_.clear();
}

void BufferBasicWordCompleter::Cancel() { suggestions_.clear(); }

void BufferBasicWordCompleter::Enable() {
    enabled_ = true;
    // AddWordsOnRange({{0, 0},
    //                  {buffer_->LineCnt() - 1,
    //                   buffer_->GetLine(buffer_->LineCnt() - 1).size()}});
}

void BufferBasicWordCompleter::Disable() {
    enabled_ = false;
    words_trie_.Clear();
}

static std::string_view BufferStringView(const Pos& pos, size_t len,
                                         Buffer* buffer) {
    return {buffer->GetLine(pos.line).c_str() + pos.byte_offset, len};
}

void BufferBasicWordCompleter::BeforeAdd(const Pos& pos,
                                         const std::string& str) {
    // Pos pos = range.begin;
    // WordRangeTree::iterator iter;
    // PosInWordRange loc = LocateInWordRange(pos, iter);
    // if (loc == PosInWordRange::kNone) {
    //     AddWordsOnRange(range);
    //     return;
    // }

    // Range for_scan;
    // if (loc == PosInWordRange::kEnd) {
    //     for_scan.begin = iter->first.begin;
    //     for_scan.end = range.end;
    // } else if (loc == PosInWordRange::kBegin) {
    //     for_scan.begin = range.begin;
    //     size_t end_byte_offset = range.end.byte_offset + iter->first.len;
    //     for_scan.end = {range.end.line, end_byte_offset};
    // } else {  // kMid
    //     for_scan.begin = iter->first.begin;
    //     for_scan.end = iter->first.EndPos();
    // }
    // words_trie_.Delete(iter->second);
    // words_ranges_.erase(iter);
    // AddWordsOnRange(for_scan);
}
void BufferBasicWordCompleter::BeforeDelete(const Range& range) {
    // WordRangeTree::iterator iter_begin;
    // WordRangeTree::iterator iter_end;
    // PosInWordRange loc_begin = LocateInWordRange(range.begin, iter_begin);
    // PosInWordRange loc_end = LocateInWordRange(range.end, iter_end);

    // std::string new_word;
    // Pos new_word_pos;
    // if (loc_begin != PosInWordRange::kNone) {
    //     new_word.append(iter_begin->second.substr(
    //         0, range.begin.byte_offset -
    //         iter_begin->first.begin.byte_offset));
    //     new_word_pos = iter_begin->first.begin;
    // }
    // if (loc_end != PosInWordRange::kNone) {
    //     new_word.append(iter_end->second.substr(
    //         range.end.byte_offset - iter_end->first.begin.byte_offset));
    //     iter_end++;
    // }
    // for (auto iter = iter_begin; iter != iter_end;) {
    //     words_trie_.Delete(iter->second);
    //     MGO_ASSERT(iter != words_ranges_.end());
    //     iter = words_ranges_.erase(iter);
    // }
    // if (!new_word.empty()) {
    //     words_trie_.Add(new_word);
    //     words_ranges_.insert({{new_word_pos, new_word.size()}, new_word});
    // }
}

void BufferBasicWordCompleter::AddWords(std::string_view str) {
    // size_t byte_offset = 0;
    // size_t byte_offset_end = str.size();
    // Character character;
    // int byte_len;
    // bool found_word_character = false;
    // while (byte_offset < byte_offset_end) {
    //     Result res = ThisCharacterInUtf8(, byte_offset, character,
    //                                      byte_len, nullptr);
    //     MGO_ASSERT(res == kOk);
    //     if (IsWordCharacter(character[0])) {
    //         if (!found_word_character) {
    //             word_range.begin = {begin.line, byte_offset};
    //             found_word_character = true;
    //         }
    //     } else {
    //         if (found_word_character) {
    //             word_range.len = byte_offset - word_range.begin.byte_offset;
    //             words_ranges_.insert(
    //                 {word_range,
    //                 std::string(word_range.StringView(buffer_))});
    //             words_trie_.Add(word_range.StringView(buffer_));
    //             found_word_character = false;
    //         }
    //     }
    //     byte_offset += byte_len;
    // }
    // if (found_word_character) {
    //     word_range.len = byte_offset_end - word_range.begin.byte_offset;
    //     words_ranges_.insert(
    //         {word_range, std::string(word_range.StringView(buffer_))});
    //     words_trie_.Add(word_range.StringView(buffer_));
    //     found_word_character = false;
    // }
}

}  // namespace mango