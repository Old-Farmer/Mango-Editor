// TODO: refactor this part.
// Target:
// 1. completer should be focus only on data, not ui and data.
// 2. peelcompleter should be more general, instead of a lot of if else.

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

// throw FSException
static std::vector<std::string> SuggestFilePath(std::string_view hint) {
    int64_t sep_index = Path::LastPathSeperator(hint);
    std::vector<std::string> paths;
    if (sep_index == -1) {
        paths = Path::ListUnderPath(".");
    } else {
        paths = Path::ListUnderPath(std::string(hint.substr(0, sep_index)));
    }
    if (paths.empty()) {
        return paths;
    }

    std::string_view filter_str;
    filter_str = sep_index == -1
                     ? hint
                     : hint.substr(sep_index + 1, hint.size() - sep_index - 1);
    if (filter_str.size() == 0) {
        return paths;
    }

    for (auto iter = paths.begin(); iter != paths.end();) {
        if (!StrFuzzyMatchInBytes(filter_str, *iter, true)) {
            iter = paths.erase(iter);
        } else {
            iter++;
        }
    }
    return paths;
}

static std::vector<std::string> SuggestBuffers(std::string_view hint,
                                               BufferManager* buffer_manager) {
    std::vector<std::string> ret;
    for (Buffer* buffer = buffer_manager->Begin();
         buffer != buffer_manager->End(); buffer = buffer->next_) {
        if (hint.empty() ||
            StrFuzzyMatchInBytes(hint, buffer->Name(), true)) {
            ret.emplace_back(buffer->Name());
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
        this_arg_offset_ = args.back().data() - content_before_cursor.data();
    }

    if (arg_index == 0) {
        // TODO: suggest commands
    } else {
        if (args[0] == "e") {
            if (arg_index == 1) {
                try {
                    suggestions_ = SuggestFilePath(arg_hint);
                } catch (FSException& e) {
                    // TODO: maybe we can throw catch the outer?
                    MGO_LOG_ERROR("%s", e.what());
                }
                type_ = SuggestType::kPath;
            }
        } else if (args[0] == "b") {
            if (arg_index == 1) {
                suggestions_ = SuggestBuffers(arg_hint, buffer_manager_);
                type_ = SuggestType::kBuffer;
            }
        }
    }
    menu_entries = suggestions_;
}
Result PeelCompleter::Accept(size_t index, Cursor* cursor) {
    Pos pos;
    peel_->frame_.make_cursor_visible_ = true;
    if (this_arg_offset_ == cursor->byte_offset) {
        peel_->frame_.buffer_->Add({0, cursor->byte_offset},
                                   suggestions_[index], nullptr, false, pos);
    } else if (type_ == SuggestType::kBuffer) {
        peel_->frame_.buffer_->Replace(
            {{0, this_arg_offset_}, {0, cursor->byte_offset}},
            suggestions_[index], nullptr, false, pos);
    } else {
        std::string_view hint = {peel_->GetContent().c_str() + this_arg_offset_,
                                 cursor->byte_offset - this_arg_offset_};
        int64_t sep_index = Path::LastPathSeperator(hint);
        if (sep_index == static_cast<int64_t>(hint.size() - 1)) {
            peel_->frame_.buffer_->Add({0, cursor->byte_offset},
                                       suggestions_[index], nullptr, false,
                                       pos);
        } else {
            peel_->frame_.buffer_->Replace(
                {{0, sep_index + 1 + this_arg_offset_},
                 {0, cursor->byte_offset}},
                suggestions_[index], nullptr, false, pos);
        }
    }
    cursor->SetPos(pos);
    suggestions_.clear();
    return type_ == SuggestType::kPath ? kRetriggerCmp : kOk;
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

Result BufferBasicWordCompleter::Accept(size_t index, Cursor* cursor) {
    MGO_ASSERT(cursor->in_window);
    MGO_ASSERT(index < suggestions_.size());
    Pos cursor_pos = cursor->ToPos();
    cursor->in_window->Replace(
        {{cursor_pos.line,
          cursor_pos.byte_offset - bytes_of_word_before_cursor_},
         cursor_pos},
        std::move(suggestions_[index]));  // Ignore ret
    suggestions_.clear();
    return kOk;
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