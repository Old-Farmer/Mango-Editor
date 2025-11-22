#pragma once

#include <unordered_map>

#include "buffer.h"
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

namespace mango {
struct Highlight {
    Range range;
    Terminal::AttrPair attr;
};

struct SyntaxContext {
    TSTree* tree;
    std::vector<Highlight> syntax_highlight;
    std::vector<int64_t> syntax_priority;
};

class SyntaxParser {
   public:
    SyntaxParser(GlobalOpts* options);
    ~SyntaxParser();
    MGO_DELETE_COPY(SyntaxParser);
    MGO_DELETE_MOVE(SyntaxParser);

    void SyntaxInit(const Buffer* buffer);
    void ParseSyntaxAfterEdit(Buffer* buffer);
    void OnBufferDelete(const Buffer* buffer);
    // Get buffer Syntax Context: Current is Buffer syntax hl info.
    // Need provide a range, so this function can calculate syntax hl info in the range.
    // This range should be as small as possible.
    // throw TSQueryPredicateDirectiveNotSupportException
    const SyntaxContext* GetBufferSyntaxContext(const Buffer* buffer,
                                                const Range& range);

   private:
    struct TSQueryPatternContext {
        regex_t match;  // for match? predicate
        bool match_init = false;

        ~TSQueryPatternContext() {
            if (match_init) {
                regfree(&match);
            }
        }
    };

    struct TSQueryContext {
        TSQuery* query;
        std::vector<std::unique_ptr<TSQueryPatternContext>> pattern_context;
    };

    // throw TSQueryPredicateDirectiveNotSupportException
    void InitQueryContex(TSQueryContext& query_context);
    // throw TSQueryPredicateDirectiveNotSupportException
    const TSQueryContext* GetQueryContext(zstring_view filetype);

    // throw TSQueryPredicateDirectiveNotSupportException
    void GenerateHighlight(const Buffer* buffer, const Range& range);
    // return true to indicate that predicate ok
    bool QueryPredicate(const TSQueryContext& query_context,
                        const TSQueryCapture* capture, const Buffer* buffer,
                        const Range& range);

    std::unordered_map<std::string_view, TSQueryContext> filetype_to_query_;
    std::unordered_map<zstring_view, const TSLanguage*> filetype_to_language_;
    const std::unordered_map<std::string_view, ColorSchemeType>*
        ts_query_capture_name_to_character_type_;

    static constexpr int kTSCaptureNamePropertyLowest = INT_MAX;

    std::unordered_map<int64_t, SyntaxContext> buffer_context_;
    TSParser* parser_ = nullptr;
    TSQueryCursor* query_cursor_;

    GlobalOpts* global_opts_;
};

}  // namespace mango