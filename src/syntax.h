#pragma once

#include <unordered_map>

#include "buffer.h"
#include "term.h"
#include "utils.h"

struct TSParser;
struct TSTree;
struct TSQuery;
struct TSLanguage;
struct TSQueryCursor;

namespace mango {
struct Highlight {
    Range range;
    Terminal::AttrPair attr;
    int8_t priority;
};

struct SyntaxContext {
    TSTree* tree;
    std::vector<Highlight> syntax_highlight;
};

class SyntaxParser {
   public:
    SyntaxParser(Options* options);
    ~SyntaxParser();
    MANGO_DELETE_COPY(SyntaxParser);
    MANGO_DELETE_MOVE(SyntaxParser);

    void HighlightInit(const Buffer* buffer);
    void HighlightAfterEdit(Buffer* buffer);
    void OnBufferDelete(const Buffer* buffer);
    SyntaxContext* GetBufferSyntaxContext(const Buffer* buffer);

   private:
    TSQuery* GetQuery(zstring_view filetype);
    void GenerateHighlight(TSQuery* query, TSTree* tree, const Buffer* buffer);

    std::unordered_map<std::string_view, TSQuery*> filetype_to_query_;
    std::unordered_map<zstring_view, const TSLanguage*> filetype_to_language_;

    std::unordered_map<int64_t, SyntaxContext> buffer_context_;
    TSParser* parser_ = nullptr;
    TSQueryCursor* query_cursor_;

    Options* options_;
};

}  // namespace mango