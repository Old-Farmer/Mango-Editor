#include "completer.h"

#include <unordered_set>

#include "buffer.h"
#include "buffer_manager.h"
#include "character.h"
#include "cursor.h"
#include "mango_peel.h"
#include "str.h"
#include "window.h"

namespace mango {

PeelCompleter::PeelCompleter(MangoPeel* peel, BufferManager* buffer_manager)
    : peel_(peel), buffer_manager_(buffer_manager) {}

static std::vector<std::string> SuggestFilePath(std::string_view hint) {
    return {};
}

static std::vector<std::string> SuggestBuffers(std::string_view hint,
                                               BufferManager* buffer_manager) {
    std::vector<std::string> ret;
    for (Buffer* buffer = buffer_manager->Begin();
         buffer != buffer_manager->End(); buffer = buffer->next_) {
        if (hint.empty() ||
            StrFuzzyMatchInBytes(hint, buffer->path().ThisPath(), true)) {
            ret.emplace_back(buffer->path().ThisPath());
        }
    }
    return ret;
}

void PeelCompleter::Suggest(const Pos& cursor_pos,
                            std::vector<std::string>& menu_entries) {
    const std::string_view content_before_cursor{peel_->GetContent().data(),
                                                 cursor_pos.byte_offset};
    auto args = StrSplit(content_before_cursor);
    std::string_view arg_hint;
    size_t arg_index;
    if (content_before_cursor.empty()) {
        arg_index = 0;
        this_arg_offset_ = 0;
    } else if (content_before_cursor.back() == kSpaceChar) {
        arg_index = args.size();
        this_arg_offset_ = cursor_pos.byte_offset;
    } else {
        arg_index = args.size() - 1;
        arg_hint = args.back();
        this_arg_offset_ = args.back().data() - content_before_cursor.data() -
                           args.back().size();
    }

    if (arg_index == 0) {
        // TODO: suggest commands
    } else {
        if (args[0] == "e") {
            suggestions_ = SuggestFilePath(arg_hint);
        } else if (args[0] == "b") {
            suggestions_ = SuggestBuffers(arg_hint, buffer_manager_);
        }
    }
    menu_entries = suggestions_;
}
void PeelCompleter::Accept(size_t index, Cursor* cursor) {
    Pos pos;
    if (this_arg_offset_ == cursor->byte_offset) {
        peel_->frame_.buffer_->Add({0, cursor->byte_offset},
                                   suggestions_[index], nullptr, false, pos);
    } else {
        peel_->frame_.buffer_->Replace(
            {{0, this_arg_offset_}, {0, cursor->byte_offset}},
            suggestions_[index], nullptr, false, pos);
    }
    cursor->SetPos(pos);
    suggestions_.clear();
}

void PeelCompleter::Cancel() { suggestions_.clear(); }

BufferBasicWordCompleter::BufferBasicWordCompleter(const Buffer* buffer) {
    buffer_ = buffer;
}

void BufferBasicWordCompleter::Suggest(const Pos& cursor_pos,
                                       std::vector<std::string>& menu_entries) {
    int64_t byte_offset = cursor_pos.byte_offset;
    const std::string& cur_line = buffer_->GetLine(cursor_pos.line);
    Character character;
    int byte_len;
    while (byte_offset > 0) {
        Result res = PrevCharacter(cur_line, byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordSeperator(c)) {
            break;
        }
        byte_offset -= byte_len;
    }
    // We need at least one word character before cursor to trigger suggestion.
    if (static_cast<size_t>(byte_offset) == cursor_pos.byte_offset) {
        menu_entries = {};
        return;
    }

    std::string_view cur_word_prefix = {cur_line.c_str() + byte_offset,
                                        cursor_pos.byte_offset - byte_offset};

    // auto now = std::chrono::steady_clock::now();

    // TODO: Performance: line cache.
    // TODO: Maybe we should lock text before accpet or cancel?
    // NOTE: The current implementation is optimized by the buffer line array
    // model, so store string view is safe.
    std::unordered_set<std::string_view> s;
    size_t lines = buffer_->LineCnt();
    for (size_t i = 0; i < lines; i++) {
        const std::string& line = buffer_->GetLine(i);
        auto words = GetWords(line);
        for (auto word : words) {
            if (s.count(word)) {
                continue;
            }
            if (StrFuzzyMatchInBytes(cur_word_prefix, word, true)) {
                s.insert(word);
            }
        }
    }
    // auto end = std::chrono::steady_clock::now();
    // MGO_LOG_INFO(
    //     "time %ld ms",
    //     std::chrono::duration_cast<std::chrono::microseconds>(end - now)
    //         .count());

    menu_entries = std::vector<std::string>(s.begin(), s.end());
    suggestions_ = menu_entries;
    bytes_of_word_before_cursor_ = cur_word_prefix.size();
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

void BufferBasicWordCompleter::Enable() { enabled_ = true; }

void BufferBasicWordCompleter::Disable() { enabled_ = false; }

// TODO: maybe we can scan codepoint instead of grapheme on big files?
std::vector<std::string_view> BufferBasicWordCompleter::GetWords(
    std::string_view str) {
    std::vector<std::string_view> words;

    Character character;
    size_t byte_offset = 0;
    size_t byte_offset_end = str.size();
    bool found_word_character = false;
    size_t word_begin;
    int byte_len;
    char c;
    while (byte_offset < byte_offset_end) {
        ThisCharacterInline(str, byte_offset, character, byte_len);
        if (character.Ascii(c) && IsWordSeperator(c)) {
            if (found_word_character) {
                words.push_back(
                    {str.data() + word_begin, byte_offset - word_begin});
                found_word_character = false;
            }
        } else {
            if (!found_word_character) {
                word_begin = byte_offset;
                found_word_character = true;
            }
        }
        byte_offset += byte_len;
    }
    if (found_word_character) {
        words.push_back({str.data() + word_begin, byte_offset - word_begin});
    }
    return words;
}

}  // namespace mango