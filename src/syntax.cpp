#include "syntax.h"

#include <cinttypes>

#include "character.h"
#include "constants.h"
#include "options.h"
#include "tree_sitter/api.h"

extern "C" {
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_cpp(void);
const TSLanguage* tree_sitter_json(void);
}

namespace mango {

namespace {

// TODO: This part is a bull shit, organize it better.
const std::unordered_map<std::string_view, std::string>
    kFiletypeToQueryFilePath = {
        {"c",
         "third-party/tree-sitter-grammars/tree-sitter-c/queries/"
         "highlights.scm"},
        {"cpp",
         "third-party/tree-sitter-grammars/tree-sitter-cpp/queries/"
         "highlights.scm"},
        {"json",
         "third-party/tree-sitter-grammars/tree-sitter-json/queries/"
         "highlights.scm"},
};

const char* my_ts_read(void* payload, uint32_t byte_offset, TSPoint position,
                       uint32_t* bytes_read) {
    (void)byte_offset;
    Buffer* buffer = reinterpret_cast<Buffer*>(payload);
    if (position.row < buffer->LineCnt() - 1) {
        if (position.column == buffer->lines()[position.row].line_str.size()) {
            *bytes_read = 1;
            return kNewLine;
        }
        *bytes_read =
            buffer->lines()[position.row].line_str.size() - position.column;
        return buffer->lines()[position.row].line_str.c_str() + position.column;
    }

    if (position.row == buffer->LineCnt() - 1) {
        if (position.column == buffer->lines()[position.row].line_str.size()) {
            *bytes_read = 0;
            return nullptr;
        }
        *bytes_read =
            buffer->lines()[position.row].line_str.size() - position.column;
        return buffer->lines()[position.row].line_str.c_str() + position.column;
    }

    // >=
    // shoudn't get here
    *bytes_read = 0;
    return nullptr;
}

}  // namespace

SyntaxParser::SyntaxParser(Options* options)
    : parser_(ts_parser_new()),
      query_cursor_(ts_query_cursor_new()),
      options_(options) {
    filetype_to_language_["c"] = tree_sitter_c();
    filetype_to_language_["cpp"] = tree_sitter_cpp();
    filetype_to_language_["json"] = tree_sitter_json();
}

SyntaxParser::~SyntaxParser() {
    ts_query_cursor_delete(query_cursor_);
    ts_parser_delete(parser_);

    for (auto& item : filetype_to_query_) {
        ts_query_delete(item.second);
    }
}

void SyntaxParser::GenerateHighlight(TSQuery* query, TSTree* tree,
                                     const Buffer* buffer) {
    TSNode root = ts_tree_root_node(tree);
    ts_query_cursor_exec(query_cursor_, query, root);
    assert(buffer_context_.count(buffer->id()) == 1);
    SyntaxContext& context = buffer_context_[buffer->id()];
    TSQueryMatch match;
    context.syntax_highlight.clear();
    while (true) {
        bool match_ok = ts_query_cursor_next_match(query_cursor_, &match);
        if (!match_ok) {
            break;
        }
        for (size_t i = 0; i < match.capture_count; i++) {
            uint32_t len;
            const char* name = ts_query_capture_name_for_id(
                query, match.captures[i].index, &len);
            TSPoint start = ts_node_start_point(match.captures[i].node);
            TSPoint end = ts_node_end_point(match.captures[i].node);
            // MANGO_LOG_DEBUG("capture name: %s, range: [(%" PRIu32 ", %"
            // PRIu32
            //                 "), (%" PRIu32 ", %" PRIu32 "))",
            //                 name, start.row, start.column, end.row,
            //                 end.column);

            // TODO: optimize string compare
            Terminal::AttrPair attr;
            if (strcmp(name, kKeywordStr) == 0) {
                attr = options_->attr_table[kKeyword];
            } else if (strcmp(name, kStringStr) == 0) {
                attr = options_->attr_table[kString];
            } else if (strcmp(name, kCommentStr) == 0) {
                attr = options_->attr_table[kComment];
            } else if (strcmp(name, kNumberStr) == 0) {
                attr = options_->attr_table[kNumber];
            } else {
                continue;
            }
            context.syntax_highlight.push_back(
                {{{start.row, start.column}, {end.row, end.column}}, attr});
        }
    }
}

void SyntaxParser::SyntaxHighlightInit(const Buffer* buffer) {
    std::vector<Highlight> highlight;
    auto filetype = buffer->filetype();
    TSQuery* query = GetQuery(filetype);
    if (query == nullptr) {
        return;
    }

    if (!ts_parser_set_language(parser_,
                                filetype_to_language_.at(buffer->filetype()))) {
        MANGO_LOG_ERROR("ts_parser_set_language error: filetype %s",
                        zstring_view_c_str(buffer->filetype()));
        return;
    }

    TSInput input = {const_cast<Buffer*>(buffer), my_ts_read,
                     TSInputEncodingUTF8, nullptr};
    TSTree* tree = ts_parser_parse(parser_, nullptr, input);
    if (tree == nullptr) {
        MANGO_LOG_ERROR("ts_parser_parse error: filetype %s",
                        zstring_view_c_str(buffer->filetype()));
        return;
    }
    buffer_context_[buffer->id()].tree = tree;
    GenerateHighlight(query, tree, buffer);
}

void SyntaxParser::SyntaxHighlightAfterEdit(Buffer* buffer) {
    auto iter = buffer_context_.find(buffer->id());
    if (iter == buffer_context_.end()) {
        return;
    }
    SyntaxContext& context = iter->second;
    TSInputEdit ts_edit = buffer->GetEditForTreeSitter();
    ts_tree_edit(context.tree, &ts_edit);
    TSInput input = {const_cast<Buffer*>(buffer), my_ts_read,
                     TSInputEncodingUTF8, nullptr};
    context.tree = ts_parser_parse(parser_, context.tree, input);
    if (context.tree == nullptr) {
        MANGO_LOG_ERROR("ts_parser_parse error: filetype %s",
                        zstring_view_c_str(buffer->filetype()));
        return;
    }
    GenerateHighlight(filetype_to_query_[buffer->filetype()], context.tree,
                      buffer);
}

void SyntaxParser::OnBufferDelete(const Buffer* buffer) {
    auto iter = buffer_context_.find(buffer->id());
    if (iter == buffer_context_.end()) {
        return;
    }

    ts_tree_delete(iter->second.tree);
    buffer_context_.erase(iter);
}

const SyntaxContext* SyntaxParser::GetBufferSyntaxContext(
    const Buffer* buffer) {
    auto iter = buffer_context_.find(buffer->id());
    if (iter == buffer_context_.end()) {
        return nullptr;
    }

    return &iter->second;
}

TSQuery* SyntaxParser::GetQuery(zstring_view filetype) {
    TSQuery* query;
    auto iter = filetype_to_query_.find(filetype);
    if (iter == filetype_to_query_.end()) {
        auto iter_ft2query_fpath = kFiletypeToQueryFilePath.find(filetype);
        if (iter_ft2query_fpath == kFiletypeToQueryFilePath.end()) {
            return nullptr;
        }
        try {
            File f(Path::GetAppRoot() + iter_ft2query_fpath->second, "r",
                   false);
            std::string query_str = f.ReadAll();
            // Cpp need C
            if (filetype == "cpp") {
                auto iter_ft2query_fpath_c = kFiletypeToQueryFilePath.find("c");
                if (iter_ft2query_fpath_c != kFiletypeToQueryFilePath.end()) {
                    File f2(Path::GetAppRoot() + iter_ft2query_fpath_c->second,
                            "r", false);
                    std::string query_str_c = f2.ReadAll();
                    query_str += kNewLine + query_str_c;
                }
            }
            uint32_t error_offset;
            TSQueryError error_type;
            query = ts_query_new(filetype_to_language_.at(filetype),
                                 query_str.c_str(), query_str.size(),
                                 &error_offset, &error_type);
            if (query == nullptr) {
                MANGO_LOG_ERROR("ts query create error: offset %" PRIu32
                                " error %d",
                                error_offset, error_type);
                return nullptr;
            }
            filetype_to_query_[filetype] = query;
        } catch (IOException& e) {
            MANGO_LOG_ERROR(
                "file %s cannot read: %s",
                (Path::GetAppRoot() + iter_ft2query_fpath->second)
                    .c_str(),
                e.what());
            return nullptr;
        } catch (std::out_of_range& e) {
            MANGO_LOG_ERROR(
                "tree-sitter TSLanguage create function not defined");
            return nullptr;
        }
    } else {
        query = iter->second;
    }
    return query;
}

}  // namespace mango