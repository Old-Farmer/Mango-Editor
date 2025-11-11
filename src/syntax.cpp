#include "syntax.h"

#include <cinttypes>

#include "constants.h"
#include "exception.h"
#include "options.h"
#include "tree_sitter/api.h"

// TODO: refactor here
extern "C" {
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_cpp(void);
const TSLanguage* tree_sitter_json(void);
}

namespace mango {

constexpr const char* kTSNewLine = "\n";

namespace {

std::string QueryFilePath(zstring_view filetype) {
    return Path::GetAppRoot() + kTSQueryPath + zstring_view_c_str(filetype) +
           "/highlights.scm";
}

const char* my_ts_read(void* payload, uint32_t byte_offset, TSPoint position,
                       uint32_t* bytes_read) {
    (void)byte_offset;
    Buffer* buffer = reinterpret_cast<Buffer*>(payload);
    if (position.row < buffer->LineCnt() - 1) {
        if (position.column == buffer->lines()[position.row].line_str.size()) {
            *bytes_read = 1;
            return kTSNewLine;
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

struct CharacterTypeCaptureNameMappingItem {
    CharacterType t;
    std::vector<std::string_view> capture_names;
};

const std::vector<CharacterTypeCaptureNameMappingItem>
    kCharacterTypeToTSQueryCaptureName = {
        {kFunction, {"function", "funtion.special"}},
        {kConstant, {"constant", "constant.builtin"}},
        {kVariable, {"variable"}},
        {kProperty, {"Property"}},
        {kNumber, {"number"}},
        {kTypeBuiltin, {"type.builtin"}},
        {kType, {"type"}},
        {kString, {"string", "string.special.key"}},
        {kComment, {"comment"}},
        {kOperator, {"operator"}},
        {kKeyword, {"keyword", "variable.builtin"}},
        {kDelimiter, {"delimiter"}},
        {kLabel, {"label"}},
};

static void SyntaxParserStaticInit(
    const std::unordered_map<std::string_view, CharacterType>*&
        ts_query_capture_name_to_character_type) {
    static std::unordered_map<std::string_view, CharacterType>
        static_ts_query_capture_name_to_character_type;

    for (auto& c_type_to_query_name : kCharacterTypeToTSQueryCaptureName) {
        for (std::string_view query_name : c_type_to_query_name.capture_names) {
            static_ts_query_capture_name_to_character_type[query_name] =
                c_type_to_query_name.t;
        }
    }

    ts_query_capture_name_to_character_type =
        &static_ts_query_capture_name_to_character_type;
}

}  // namespace

SyntaxParser::SyntaxParser(Options* options)
    : parser_(ts_parser_new()),
      query_cursor_(ts_query_cursor_new()),
      options_(options) {
    // TODO: refactor here
    filetype_to_language_["c"] = tree_sitter_c();
    filetype_to_language_["cpp"] = tree_sitter_cpp();
    filetype_to_language_["json"] = tree_sitter_json();

    SyntaxParserStaticInit(ts_query_capture_name_to_character_type_);
}

SyntaxParser::~SyntaxParser() {
    ts_query_cursor_delete(query_cursor_);
    ts_parser_delete(parser_);

    for (auto& item : filetype_to_query_) {
        ts_query_delete(item.second.query);
    }
}

bool SyntaxParser::QueryPredicate(const TSQueryContext& query_context,
                                  const TSQueryCapture* capture,
                                  const Buffer* buffer, const Range& range) {
    uint32_t predicates_steps;
    const TSQueryPredicateStep* predicates = ts_query_predicates_for_pattern(
        query_context.query, capture->index, &predicates_steps);
    if (predicates_steps == 0) {
        return true;
    }
    for (uint32_t i = 0; i < predicates_steps;) {
        uint32_t str_size;
        MGO_ASSERT(predicates[i].type == TSQueryPredicateStepTypeString);

        const char* predicate = ts_query_string_value_for_id(
            query_context.query, predicates[i].value_id, &str_size);
        if (strcmp(predicate, "match?") == 0) {
            const regex_t& regex =
                query_context.pattern_context[capture->index]->match;

            if (range.begin.line == range.end.line) {
                Buffer* b = const_cast<Buffer*>(buffer);
                std::string& str = b->GetLineNonConst(range.begin.line);
                char tmp = str[range.end.byte_offset];
                str[range.end.byte_offset] = '\0';
                int exec_ret =
                    regexec(&regex, str.c_str() + range.begin.byte_offset, 0,
                            nullptr, 0);
                str[range.end.byte_offset] = tmp;
                if (exec_ret == REG_NOMATCH) {
                    return false;
                }
            } else {
                std::string str = buffer->GetContent(range);
                if (REG_NOMATCH ==
                    regexec(&regex, str.c_str(), 0, nullptr, REG_NOSUB)) {
                    return false;
                }
            }
            i += 4;
        }
    }
    return true;
}

void SyntaxParser::GenerateHighlight(const TSQueryContext& query_context,
                                     TSTree* tree, const Buffer* buffer) {
    TSNode root = ts_tree_root_node(tree);
    ts_query_cursor_exec(query_cursor_, query_context.query, root);
    MGO_ASSERT(buffer_context_.count(buffer->id()) == 1);
    SyntaxContext& context = buffer_context_[buffer->id()];
    TSQueryMatch match;
    context.syntax_highlight.clear();
    std::vector<int64_t> highlights_priority;

    while (true) {
        bool match_ok = ts_query_cursor_next_match(query_cursor_, &match);
        if (!match_ok) {
            break;
        }

        // MGO_LOG_DEBUG("One Match");
        for (size_t i = 0; i < match.capture_count; i++) {
            uint32_t len;
            const char* name = ts_query_capture_name_for_id(
                query_context.query, match.captures[i].index, &len);
            TSPoint start = ts_node_start_point(match.captures[i].node);
            TSPoint end = ts_node_end_point(match.captures[i].node);
            // MGO_LOG_DEBUG("capture name: %s, index: %" PRIu32
            //               ", range: [(%" PRIu32 ", %" PRIu32 "), (%" PRIu32
            //               ", %" PRIu32 "))",
            //               name, match.captures[i].index, start.row,
            //               start.column, end.row, end.column);

            Range range = {{start.row, start.column}, {end.row, end.column}};
            if (!QueryPredicate(query_context, &match.captures[i], buffer,
                                range)) {
                continue;
            }

            // TODO: Maybe optimize string compare
            Terminal::AttrPair attr;
            int priority;
            auto iter = ts_query_capture_name_to_character_type_->find(name);
            if (iter == ts_query_capture_name_to_character_type_->end()) {
                attr = options_->attr_table[kNormal];
                priority = -1;
            } else {
                attr = options_->attr_table[iter->second];
                priority = match.captures[i].index;
            }

            if (context.syntax_highlight.empty()) {
                context.syntax_highlight.push_back({range, attr});
                highlights_priority.push_back(priority);
                continue;
            }

            // Clear overlap or same highlight area.
            // We prefer higher priority if hl area is the same.
            // Greater pattern index == higher prority.
            // TODO: Handle nested highlight, and more.
            while (!context.syntax_highlight.empty()) {
                Highlight& prev_hl = context.syntax_highlight.back();
                if (prev_hl.range.RangeAfterMe(range)) {
                    context.syntax_highlight.push_back({range, attr});
                    highlights_priority.push_back(priority);
                    break;
                } else if (prev_hl.range.RangeEqualMe(range)) {
                    if (highlights_priority.back() > priority) {
                        break;
                    }
                    prev_hl.attr = attr;
                    highlights_priority.back() = priority;
                    break;
                }
                context.syntax_highlight.pop_back();
                highlights_priority.pop_back();
            }
        }
    }
}

void SyntaxParser::SyntaxHighlightInit(const Buffer* buffer) {
    std::vector<Highlight> highlight;
    auto filetype = buffer->filetype();
    const TSQueryContext* query_context = GetQueryContext(filetype);
    if (query_context == nullptr) {
        return;
    }

    if (!ts_parser_set_language(parser_,
                                filetype_to_language_.at(buffer->filetype()))) {
        MGO_LOG_ERROR("ts_parser_set_language error: filetype %s",
                      zstring_view_c_str(buffer->filetype()));
        return;
    }

    TSInput input = {const_cast<Buffer*>(buffer), my_ts_read,
                     TSInputEncodingUTF8, nullptr};
    TSTree* tree = ts_parser_parse(parser_, nullptr, input);
    if (tree == nullptr) {
        MGO_LOG_ERROR("ts_parser_parse error: filetype %s",
                      zstring_view_c_str(buffer->filetype()));
        return;
    }
    buffer_context_[buffer->id()].tree = tree;
    GenerateHighlight(*query_context, tree, buffer);
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
        MGO_LOG_ERROR("ts_parser_parse error: filetype %s",
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

void SyntaxParser::InitQueryContex(TSQueryContext& query_context) {
    uint32_t predicates_steps;
    uint32_t pattern_cnt = ts_query_pattern_count(query_context.query);
    query_context.pattern_context.resize(pattern_cnt);

    for (uint32_t i = 0; i < pattern_cnt; i++) {
        const TSQueryPredicateStep* predicates =
            ts_query_predicates_for_pattern(query_context.query, i,
                                            &predicates_steps);
        if (predicates_steps == 0) {
            continue;
        }
        for (uint32_t j = 0; j < predicates_steps;) {
            uint32_t str_size;
            MGO_ASSERT(predicates[j].type == TSQueryPredicateStepTypeString);

            const char* predicate = ts_query_string_value_for_id(
                query_context.query, predicates[j].value_id, &str_size);
            if (strcmp(predicate, "match?") == 0) {
                MGO_ASSERT(predicates[j + 1].type ==
                           TSQueryPredicateStepTypeCapture);
                MGO_ASSERT(predicates[j + 2].type ==
                           TSQueryPredicateStepTypeString);
                MGO_ASSERT(predicates[j + 3].type ==
                           TSQueryPredicateStepTypeDone);

                const char* regex_pattern = ts_query_string_value_for_id(
                    query_context.query, predicates[j + 2].value_id, &str_size);
                query_context.pattern_context[i] =
                    std::make_unique<TSQueryPatternContext>();
                int ret = regcomp(&query_context.pattern_context[i]->match,
                                  regex_pattern, REG_EXTENDED | REG_NOSUB);
                if (ret != 0) {
                    std::vector<char> buf(128);
                    regerror(ret, &query_context.pattern_context[i]->match,
                             buf.data(), 128);
                    query_context.pattern_context[i].reset();
                    throw RegexCompileException("regex compile error %s",
                                                buf.data());
                }
                MGO_ASSERT(j + 4 == predicates_steps);
                j += 4;
            } else {
                throw TSQueryPredicateDirectiveNotSupportException(
                    "TS Query predicate/directive %s is not "
                    "suppored",
                    predicate);
            }
        }
    }
}

const SyntaxParser::TSQueryContext* SyntaxParser::GetQueryContext(
    zstring_view filetype) {
    auto iter = filetype_to_query_.find(filetype);
    if (iter == filetype_to_query_.end()) {
        std::string query_file_path = QueryFilePath(filetype);
        try {
            File f(query_file_path, "r", false);
            std::string query_str = f.ReadAll();
            // Cpp need C
            if (filetype == "cpp") {
                std::string query_str_c =
                    File(QueryFilePath("c"), "r", false).ReadAll();
                query_str = query_str_c + kTSNewLine + query_str;
            }
            uint32_t error_offset;
            TSQueryError error_type;
            TSQuery* query = ts_query_new(filetype_to_language_.at(filetype),
                                          query_str.c_str(), query_str.size(),
                                          &error_offset, &error_type);
            if (query == nullptr) {
                MGO_LOG_ERROR("ts query create error: offset %" PRIu32
                              " error %d",
                              error_offset, error_type);
                return nullptr;
            }
            TSQueryContext query_context;
            query_context.query = query;
            InitQueryContex(query_context);
            filetype_to_query_[filetype] = std::move(query_context);
            return &filetype_to_query_[filetype];
        } catch (IOException& e) {
            MGO_LOG_ERROR("TS query file %s cannot read: %s",
                          query_file_path.c_str(), e.what());
            filetype_to_query_[filetype] = {nullptr, {}};
            return nullptr;
        } catch (std::out_of_range& e) {
            MGO_LOG_ERROR("tree-sitter TSLanguage create function not defined");
            filetype_to_query_[filetype] = {nullptr, {}};
            return nullptr;
        }
    } else {
        if (iter->second.query) {
            return &iter->second;
        }
        return nullptr;
    }
}

}  // namespace mango