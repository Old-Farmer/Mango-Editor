// TODO: refactor this part.
// Target:
// 1. completer should be focus only on data, not ui and data.
// 2. peelcompleter should be more general, instead of a lot of if else.

#include "completer.h"

#include <unordered_set>

#include "buffer.h"
#include "buffer_manager.h"
#include "character.h"
#include "command_manager.h"
#include "constants.h"
#include "cursor.h"
#include "mango_peel.h"
#include "str.h"
#include "text_window.h"

namespace charxed {

// throw FSException
static std::vector<std::string> SuggestFilePath(std::string_view hint) {
    int64_t sep_index = Path::LastPathSeperator(hint);
    std::vector<std::string> paths;
    if (sep_index == -1) {
        paths = ListUnderDirectory(".");
    } else {
        auto dir = hint.substr(0, sep_index + 1);
        if (!Path::HaveHomeSymbol(dir)) {
            paths = ListUnderDirectory(std::string(dir));
        } else {
            paths = ListUnderDirectory(Path::ReplaceHomeSymbol(dir));
        }
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
        if (hint.empty() || StrFuzzyMatchInBytes(hint, buffer->Name(), true)) {
            ret.emplace_back(buffer->Name());
        }
    }
    return ret;
}

PeelCompleter::PeelCompleter(MangoPeel* peel, BufferManager* buffer_manager,
                             CommandManager* command_manager)
    : peel_(peel),
      buffer_manager_(buffer_manager),
      command_manager_(command_manager) {
    // A little bit ugly
    // TODO: refactor it.
    {
        auto handler = [this](int arg_index, std::string_view arg_hint) {
            if (arg_index == 1) {
                try {
                    suggestions_ = SuggestFilePath(arg_hint);
                } catch (FSException& e) {
                    // TODO: maybe we can throw catch the outer?
                    CHX_LOG_ERROR("{}", e.what());
                }
                type_ = SuggestType::kPath;
            };
        };
        cmd_name_to_cmp_handler_["e"] = handler;
        cmd_name_to_cmp_handler_["edit"] = handler;
        cmd_name_to_cmp_handler_["saveas"] = handler;
        cmd_name_to_cmp_handler_["sa"] = handler;
        cmd_name_to_cmp_handler_["create"] = handler;
        cmd_name_to_cmp_handler_["remove"] = handler;
        cmd_name_to_cmp_handler_["rm"] = handler;
        cmd_name_to_cmp_handler_["mkdir"] = handler;
        cmd_name_to_cmp_handler_["rmdir"] = handler;
    }
    {
        auto handler = [this](int arg_index, std::string_view arg_hint) {
            if (arg_index == 1 || arg_index == 2) {
                try {
                    suggestions_ = SuggestFilePath(arg_hint);
                } catch (FSException& e) {
                    // TODO: maybe we can throw catch the outer?
                    CHX_LOG_ERROR("{}", e.what());
                }
                type_ = SuggestType::kPath;
            };
        };
        cmd_name_to_cmp_handler_["move"] = handler;
        cmd_name_to_cmp_handler_["mv"] = handler;
    }
    {
        auto handler = [this](int arg_index, std::string_view arg_hint) {
            if (arg_index == 1) {
                suggestions_ = SuggestBuffers(arg_hint, buffer_manager_);
                type_ = SuggestType::kOther;
            }
        };
        cmd_name_to_cmp_handler_["b"] = handler;
        cmd_name_to_cmp_handler_["buffer"] = handler;
    }
    {
        auto handler = [this](int arg_index, std::string_view arg_hint) {
            if (arg_index == 1) {
                try {
                    suggestions_ =
                        ListUnderDirectory(Path::GetAppRoot() + kDocsPath);
                    if (!arg_hint.empty()) {
                        for (auto iter = suggestions_.begin();
                             iter != suggestions_.end();) {
                            if (!StrFuzzyMatchInBytes(arg_hint, *iter, true)) {
                                iter = suggestions_.erase(iter);
                            } else {
                                iter++;
                            }
                        }
                    }
                    type_ = SuggestType::kOther;
                } catch (FSException& e) {
                    CHX_LOG_ERROR("{}", e.what());
                }
            }
        };
        cmd_name_to_cmp_handler_["h"] = handler;
        cmd_name_to_cmp_handler_["help"] = handler;
    }
}

void PeelCompleter::Suggest(const Pos& cursor_pos,
                            std::vector<std::string>& menu_entries) {
    const std::string_view content_before_cursor{peel_->GetUserInput().data(),
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
        const auto& commands = command_manager_->commands();
        for (auto& command : commands) {
            if (StrFuzzyMatchInBytes(arg_hint, command->name, false)) {
                suggestions_.push_back(command->name);
            }
        }
        type_ = SuggestType::kOther;
    } else {
        auto iter = cmd_name_to_cmp_handler_.find(args[0]);
        if (iter != cmd_name_to_cmp_handler_.end()) {
            iter->second(arg_index, arg_hint);
        }
    }
    menu_entries = suggestions_;
}

Result PeelCompleter::Accept(size_t index, Cursor* cursor) {
    Result res;
    switch (type_) {
        case SuggestType::kPath: {
            std::string_view hint = {
                peel_->GetUserInput().data() + this_arg_offset_,
                cursor->pos.byte_offset - this_arg_offset_};
            int64_t sep_index = Path::LastPathSeperator(hint);
            if (sep_index == static_cast<int64_t>(hint.size() - 1)) {
                peel_->area_.AddStringAtCursorNoSelection(suggestions_[index]);
            } else {
                peel_->area_.Replace({{0, sep_index + 1 + this_arg_offset_},
                                      {0, cursor->pos.byte_offset}},
                                     suggestions_[index]);
            }
            res = suggestions_[index].back() == kPathSeperator ? kRetriggerCmp
                                                               : kOk;
            break;
        }
        case SuggestType::kOther:
            peel_->area_.Replace(
                {{0, this_arg_offset_}, {0, cursor->pos.byte_offset}},
                suggestions_[index]);
            res = kOk;
    }
    suggestions_.clear();
    return res;
}

void PeelCompleter::Cancel() { suggestions_.clear(); }

BufferBasicWordCompleter::BufferBasicWordCompleter(const Buffer* buffer) {
    buffer_ = buffer;
}

void BufferBasicWordCompleter::Suggest(const Pos& cursor_pos,
                                       std::vector<std::string>& menu_entries) {
    auto iter = buffer_->Find(cursor_pos);
    auto cursor_iter = iter;
    auto begin = buffer_->Find({cursor_pos.line, 0});
    Character character;
    while (iter != begin) {
        auto prev = PrevCharacter(iter, begin, character);
        char c;
        if (character.Ascii(c) && IsWordSeperator(c)) {
            break;
        }
        iter = prev;
    }
    // We need at least one word character before cursor to trigger suggestion.
    if (iter == cursor_iter) {
        menu_entries = {};
        return;
    }

    TextTree::TextView cur_word_prefix = {iter, cursor_iter};

    // auto now = std::chrono::steady_clock::now();

    // TODO: Performance: line cache.
    // TODO: Maybe we should lock text before accpet or cancel?
    std::unordered_set<TextTree::TextView, TextTree::TextViewHash,
                       TextTree::TextViewEqual>
        s;
    std::vector<std::string> matching_words;

    bool found_word_character = false;
    TextTree::Iterator word_begin;
    char c;
    iter = buffer_->Begin();
    auto end = buffer_->End();
    // TODO: maybe we can scan codepoint instead of grapheme on big files?
    while (iter != end) {
        auto next = NextCharacter(iter, end, character);
        if (character.Ascii(c) && IsWordSeperator(c)) {
            if (found_word_character) {
                TextTree::TextView word = {word_begin, iter};
                // Filter the word under cursor,
                // Filter the same word.
                if (cur_word_prefix.begin != word.begin && s.count(word) == 0 &&
                    StrFuzzyMatchInBytes(cur_word_prefix, word, true)) {
                    matching_words.push_back(word.ToString());
                    s.insert(word);
                }
                found_word_character = false;
            }
        } else {
            if (!found_word_character) {
                word_begin = iter;
                found_word_character = true;
            }
        }
        iter = next;
    }
    if (found_word_character) {
        TextTree::TextView word = {word_begin, iter};
        // Filter the word under cursor,
        // Filter the same word.
        if (cur_word_prefix.begin != word.begin && s.count(word) == 0 &&
            StrFuzzyMatchInBytes(cur_word_prefix, word, true)) {
            matching_words.push_back(word.ToString());
            s.insert(word);
        }
    }
    // auto end = std::chrono::steady_clock::now();
    // CHX_LOG_INFO(
    //     "time {} ms",
    //     std::chrono::duration_cast<std::chrono::microseconds>(end - now)
    //         .count());

    menu_entries = std::move(matching_words);
    suggestions_ = menu_entries;
    bytes_of_word_before_cursor_ = cur_word_prefix.Size();
}

Result BufferBasicWordCompleter::Accept(size_t index, Cursor* cursor) {
    CHX_ASSERT(cursor->t_win);
    CHX_ASSERT(index < suggestions_.size());
    Pos cursor_pos = cursor->pos;
    cursor->t_win->Replace({{cursor_pos.line, cursor_pos.byte_offset -
                                                  bytes_of_word_before_cursor_},
                            cursor_pos},
                           std::move(suggestions_[index]));  // Ignore ret
    suggestions_.clear();
    return kOk;
}

void BufferBasicWordCompleter::Cancel() { suggestions_.clear(); }

void BufferBasicWordCompleter::Enable() { enabled_ = true; }

void BufferBasicWordCompleter::Disable() { enabled_ = false; }

}  // namespace charxed
