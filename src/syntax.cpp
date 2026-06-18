#include "syntax.h"

#include "buffer.h"
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

namespace charxed {

constexpr const char* kTSNewLine = "\n";

namespace {

std::string QueryFilePath(FileType filetype) {
    auto p = Path::GetAppRoot();
    p.append(kTSQueryPath);
    p.append(FileTypesInnerStrRep(filetype));
    p.append("/highlights.scm");
    return p;
}

const char* MyTSRead(void* payload, uint32_t byte_offset, TSPoint position,
                     uint32_t* bytes_read) {
    (void)position;
    Buffer* buffer = reinterpret_cast<Buffer*>(payload);

    auto iter = buffer->Find(byte_offset);
    if (iter == buffer->End()) {
        *bytes_read = 0;
        return nullptr;
    }

    auto sv = iter.MaxContinuousData();
    *bytes_read = sv.size();
    return sv.data();
}

struct CharacterTypeCaptureNameMappingItem {
    ThemeType t;
    std::vector<std::string_view> capture_names;
};

static void SyntaxParserStaticInit(
    const std::unordered_map<std::string_view, ThemeType>*&
        ts_query_capture_name_to_character_type) {
    static std::unordered_map<std::string_view, ThemeType>*
        static_ts_query_capture_name_to_character_type = [] {
            auto ret =
                new std::unordered_map<std::string_view, ThemeType>();
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

            for (auto& c_type_to_query_name :
                 kCharacterTypeToTSQueryCaptureName) {
                for (std::string_view query_name :
                     c_type_to_query_name.capture_names) {
                    (*ret)[query_name] = c_type_to_query_name.t;
                }
            }
            return ret;
        }();

    ts_query_capture_name_to_character_type =
        static_ts_query_capture_name_to_character_type;
}

}  // namespace

SyntaxParser::SyntaxParser(GlobalOpts* global_opts)
    : parser_(ts_parser_new()),
      query_cursor_(ts_query_cursor_new()),
      global_opts_(global_opts) {
    (void)global_opts_;
    // TODO: refactor here
    filetype_to_language_[static_cast<int>(FileType::kC)] = tree_sitter_c();
    filetype_to_language_[static_cast<int>(FileType::kCpp)] = tree_sitter_cpp();
    filetype_to_language_[static_cast<int>(FileType::kJson)] =
        tree_sitter_json();

    SyntaxParserStaticInit(ts_query_capture_name_to_character_type_);
}

SyntaxParser::~SyntaxParser() {
    ts_query_cursor_delete(query_cursor_);
    ts_parser_delete(parser_);
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
    std::string buf;
    for (uint32_t i = 0; i < predicates_steps;) {
        uint32_t str_size;
        CHX_ASSERT(predicates[i].type == TSQueryPredicateStepTypeString);

        const char* predicate = ts_query_string_value_for_id(
            query_context.query, predicates[i].value_id, &str_size);
        if (strcmp(predicate, "match?") == 0) {
            const regex_t& regex =
                query_context.pattern_context[capture->index]->match;

            if (range.begin.line == range.end.line) {
                auto str = buffer->GetLine(range.begin.line, buf);
                regmatch_t m;
                // We use m.rm_so = 0 and str.data() + range.begin.byte_offset
                // to make '^' have effect
                m.rm_so = 0;
                m.rm_eo = range.end.byte_offset - range.begin.byte_offset;
                int exec_ret =
                    regexec(&regex, str.data() + range.begin.byte_offset, 1, &m,
                            REG_STARTEND);
                if (exec_ret == REG_NOMATCH) {
                    return false;
                }
            } else {
                std::string str = buffer->GetContent(range);
                regmatch_t m;
                m.rm_so = 0;
                m.rm_eo = str.size();
                if (REG_NOMATCH ==
                    regexec(&regex, str.c_str(), 1, &m, REG_STARTEND)) {
                    return false;
                }
            }
            i += 4;
        }
    }
    return true;
}

void SyntaxParser::GenerateHighlight(const Buffer* buffer, const Range& range) {
    CHX_ASSERT(filetype_to_query_[static_cast<int>(buffer->filetype())]->query);
    TSQueryContext& query_context =
        *filetype_to_query_[static_cast<int>(buffer->filetype())];
    CHX_ASSERT(buffer_context_.count(buffer->id()) == 1);
    SyntaxContext& context = buffer_context_[buffer->id()];

    TSNode root = ts_tree_root_node(buffer->ts_tree());
    TSPoint query_start, query_end;
    query_start.row = range.begin.line;
    query_start.column = range.begin.byte_offset;
    query_end.row = range.end.line;
    query_end.column = range.end.byte_offset;
    bool set_range_ret =
        ts_query_cursor_set_point_range(query_cursor_, query_start, query_end);
    if (!set_range_ret) {
        CHX_LOG_INFO(
            "ts_query_cursor_set_point_range error: start row {}, start col "
            "{}, end row {}, end col {}",
            query_start.row, query_start.column, query_end.row,
            query_end.column);
        return;
    }

    ts_query_cursor_exec(query_cursor_, query_context.query, root);
    TSQueryMatch match;
    context.syntax_highlight.clear();
    context.syntax_priority.clear();

    while (true) {
        bool match_ok = ts_query_cursor_next_match(query_cursor_, &match);
        if (!match_ok) {
            break;
        }

        // CHX_LOG_DEBUG("One Match");
        for (size_t i = 0; i < match.capture_count; i++) {
            uint32_t len;
            const char* name = ts_query_capture_name_for_id(
                query_context.query, match.captures[i].index, &len);
            TSPoint start = ts_node_start_point(match.captures[i].node);
            TSPoint end = ts_node_end_point(match.captures[i].node);
            // CHX_LOG_DEBUG(
            //     "capture name: {}, index: {}, range: [({}, {}), ({}, {}))",
            //     name, match.captures[i].index, start.row, start.column,
            //     end.row, end.column);

            Range range = {{start.row, start.column}, {end.row, end.column}};
            CHX_ASSERT(buffer->LineCnt() > range.end.line);
            CHX_ASSERT(range.begin.byte_offset <=
                       buffer->GetLineView(range.begin.line).Size());
            CHX_ASSERT(range.end.byte_offset <=
                       buffer->GetLineView(range.end.line).Size());
            if (!QueryPredicate(query_context, &match.captures[i], buffer,
                                range)) {
                continue;
            }

            // TODO: Maybe optimize string compare
            ThemeType hl_type;
            int64_t priority;
            auto iter = ts_query_capture_name_to_character_type_->find(name);
            if (iter == ts_query_capture_name_to_character_type_->end()) {
                hl_type = kNormalFg;
                priority = -1;
            } else {
                hl_type = iter->second;
                priority = match.captures[i].index;
            }

            if (context.syntax_highlight.empty()) {
                context.syntax_highlight.push_back({range, hl_type});
                context.syntax_priority.push_back(priority);
                continue;
            }

            // Clear overlap or same highlight area.
            // We prefer higher priority if hl area is the same.
            // Greater pattern index == higher prority.
            // TODO: Handle nested highlight, and more.
            // TODO: Cache highlights info if buffer not modified.
            while (!context.syntax_highlight.empty()) {
                Highlight& prev_hl = context.syntax_highlight.back();
                if (prev_hl.range.RangeAfterMe(range)) {
                    context.syntax_highlight.push_back({range, hl_type});
                    context.syntax_priority.push_back(priority);
                    break;
                } else if (prev_hl.range.RangeEqualMe(range)) {
                    if (context.syntax_priority.back() > priority) {
                        break;
                    }
                    prev_hl.hl_type = hl_type;
                    context.syntax_priority.back() = priority;
                    break;
                }
                context.syntax_highlight.pop_back();
                context.syntax_priority.pop_back();
            }
        }
    }
}

TSTree* SyntaxParser::SyntaxInit(const Buffer* buffer) {
    auto filetype = buffer->filetype();
    const TSQueryContext* query_context = GetQueryContext(filetype);
    if (query_context == nullptr) {
        return nullptr;
    }

    if (!ts_parser_set_language(
            parser_,
            filetype_to_language_[static_cast<int>(buffer->filetype())])) {
        CHX_LOG_ERROR("ts_parser_set_language error: filetype {}",
                      FileTypesInnerStrRep(buffer->filetype()));
        return nullptr;
    }

    TSInput input = {const_cast<Buffer*>(buffer), MyTSRead, TSInputEncodingUTF8,
                     nullptr};
    TSTree* tree = ts_parser_parse(parser_, nullptr, input);
    if (tree == nullptr) {
        CHX_LOG_ERROR("ts_parser_parse error: filetype {}",
                      FileTypesInnerStrRep(buffer->filetype()));
        return nullptr;
    }
    buffer_context_[buffer->id()] = {};
    return tree;
}

void SyntaxParser::ParseSyntaxAfterEdit(Buffer* buffer) {
    auto iter = buffer_context_.find(buffer->id());
    if (iter == buffer_context_.end()) {
        return;
    }
    TSInput input = {const_cast<Buffer*>(buffer), MyTSRead, TSInputEncodingUTF8,
                     nullptr};
    TSTree* new_tree = ts_parser_parse(parser_, buffer->ts_tree(), input);
    ts_tree_delete(buffer->ts_tree());
    buffer->ts_tree() = new_tree;
    if (buffer->ts_tree() == nullptr) {
        CHX_LOG_ERROR("ts_parser_parse error: filetype {}",
                      FileTypesInnerStrRep(buffer->filetype()));
    }
}

void SyntaxParser::OnBufferDelete(const Buffer* buffer) {
    auto iter = buffer_context_.find(buffer->id());
    if (iter == buffer_context_.end()) {
        return;
    }

    buffer_context_.erase(iter);
}

const SyntaxContext* SyntaxParser::GetBufferSyntaxContext(const Buffer* buffer,
                                                          const Range& range) {
    auto iter = buffer_context_.find(buffer->id());
    if (iter == buffer_context_.end()) {
        return nullptr;
    }

    GenerateHighlight(buffer, range);
    return &iter->second;
}

SyntaxParser::TSQueryContext::~TSQueryContext() {
    if (query) {
        ts_query_delete(query);
    }
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
            CHX_ASSERT(predicates[j].type == TSQueryPredicateStepTypeString);

            const char* predicate = ts_query_string_value_for_id(
                query_context.query, predicates[j].value_id, &str_size);
            if (strcmp(predicate, "match?") == 0) {
                CHX_ASSERT(predicates[j + 1].type ==
                           TSQueryPredicateStepTypeCapture);
                CHX_ASSERT(predicates[j + 2].type ==
                           TSQueryPredicateStepTypeString);
                CHX_ASSERT(predicates[j + 3].type ==
                           TSQueryPredicateStepTypeDone);

                const char* regex_pattern = ts_query_string_value_for_id(
                    query_context.query, predicates[j + 2].value_id, &str_size);
                query_context.pattern_context[i] =
                    std::make_unique<TSQueryPatternContext>();
                int ret = regcomp(&query_context.pattern_context[i]->match,
                                  regex_pattern, REG_EXTENDED);
                if (ret != 0) {
                    char buf[128];
                    regerror(ret, &query_context.pattern_context[i]->match, buf,
                             128);
                    query_context.pattern_context[i].reset();
                    throw RegexCompileException("regex compile error {}", buf);
                }
                CHX_ASSERT(j + 4 == predicates_steps);
                j += 4;
            } else {
                throw TSQueryPredicateDirectiveNotSupportException(
                    "TS Query predicate/directive {} is not "
                    "supported",
                    predicate);
            }
        }
    }
}

const SyntaxParser::TSQueryContext* SyntaxParser::GetQueryContext(
    FileType filetype) {
    auto& lang = filetype_to_language_[static_cast<int>(filetype)];
    auto& query_context = filetype_to_query_[static_cast<int>(filetype)];
    if (lang == nullptr) {
        CHX_LOG_ERROR("tree-sitter TSLanguage {} create function not defined",
                      FileTypesInnerStrRep(filetype));
        query_context = std::make_unique<TSQueryContext>();
        return nullptr;
    }

    if (!query_context) {
        std::string query_file_path = QueryFilePath(filetype);
        try {
            File f(query_file_path, "r");
            EOLSeq eol_seq;
            std::string query_str = f.ReadAll(eol_seq);
            // Cpp need C
            if (filetype == FileType::kCpp) {
                std::string query_str_c =
                    File(QueryFilePath(FileType::kC), "r").ReadAll(eol_seq);
                query_str = query_str_c + kTSNewLine + query_str;
            }
            uint32_t error_offset;
            TSQueryError error_type;
            TSQuery* query =
                ts_query_new(lang, query_str.c_str(), query_str.size(),
                             &error_offset, &error_type);
            if (query == nullptr) {
                CHX_LOG_ERROR("ts query create error: offset {}, error {}",
                              error_offset, static_cast<int>(error_type));
                return nullptr;
            }
            query_context = std::make_unique<TSQueryContext>();
            query_context->query = query;
            InitQueryContex(*query_context);
            return query_context.get();
        } catch (IOException& e) {
            CHX_LOG_ERROR("TS query file {} cannot read: {}", query_file_path,
                          e.what());
            query_context = std::make_unique<TSQueryContext>();
            return nullptr;
        }
    } else {
        if (query_context->query) {
            return query_context.get();
        }
        return nullptr;
    }
}

}  // namespace charxed
