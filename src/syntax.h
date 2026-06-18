#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "options.h"
#include "regex.h"
#include "term.h"
#include "utils.h"

struct TSParser;
struct TSTree;
struct TSQuery;
struct TSLanguage;
struct TSQueryCursor;
struct TSQueryCapture;
struct TSInputEdit;

namespace charxed {

class Buffer;

struct Highlight {
    Range range;
    ThemeType hl_type;
};

// TODO: maybe we don't need pre buffer hl context? remove it?
struct SyntaxContext {
    std::vector<Highlight> syntax_highlight;
    std::vector<int64_t> syntax_priority;
};

class SyntaxParser {
   public:
    SyntaxParser(GlobalOpts* options);
    ~SyntaxParser();
    CHX_DELETE_COPY(SyntaxParser);
    CHX_DELETE_MOVE(SyntaxParser);

    // return nullptr means init fail
    TSTree* SyntaxInit(const Buffer* buffer);
    void ParseSyntaxAfterEdit(Buffer* buffer);
    void OnBufferDelete(const Buffer* buffer);
    // Get buffer Syntax Context: Current is Buffer syntax hl info.
    // Need provide a range, so this function can calculate syntax hl info in
    // the range. This range should be as small as possible. throw
    // TSQueryPredicateDirectiveNotSupportException
    const SyntaxContext* GetBufferSyntaxContext(const Buffer* buffer,
                                                const Range& range);

   private:
    struct TSQueryPatternContext {
        regex_t match;  // for match? predicate
        bool match_init = false;

        TSQueryPatternContext() {};
        ~TSQueryPatternContext() {
            if (match_init) {
                regfree(&match);
            }
        }
    };

    struct TSQueryContext {
        TSQuery* query = nullptr;
        std::vector<std::unique_ptr<TSQueryPatternContext>> pattern_context;

        CHX_DELETE_COPY(TSQueryContext);
        CHX_DELETE_MOVE(TSQueryContext);

        TSQueryContext() {};
        ~TSQueryContext();
    };

    // throw TSQueryPredicateDirectiveNotSupportException
    void InitQueryContex(TSQueryContext& query_context);
    // throw TSQueryPredicateDirectiveNotSupportException
    const TSQueryContext* GetQueryContext(FileType filetype);

    // throw TSQueryPredicateDirectiveNotSupportException
    void GenerateHighlight(const Buffer* buffer, const Range& range);
    // return true to indicate that predicate ok
    bool QueryPredicate(const TSQueryContext& query_context,
                        const TSQueryCapture* capture, const Buffer* buffer,
                        const Range& range);

    std::unique_ptr<TSQueryContext>
        filetype_to_query_[static_cast<int>(FileType::_kCount)];
    const TSLanguage* filetype_to_language_[static_cast<int>(
        FileType::_kCount)] = {};  // all nullptr
    const std::unordered_map<std::string_view, ThemeType>*
        ts_query_capture_name_to_character_type_;

    static constexpr int kTSCaptureNamePropertyLowest = INT_MAX;

    std::unordered_map<int64_t, SyntaxContext> buffer_context_;
    TSParser* parser_ = nullptr;
    TSQueryCursor* query_cursor_;

    GlobalOpts* global_opts_;
};

}  // namespace charxed
