#pragma once

#include <cstdint>
#include <string>

#include "pos.h"
#include "result.h"
#include "search.h"
namespace charxed {

// An interface that summarizes all general window functionalities.
class Window {
   public:
    virtual ~Window() = default;
    virtual bool IsSelectionActive() { return false; };
    // return true if success
    virtual bool StartSelection(Pos anchor) {
        (void)anchor;
        return false;
    };
    virtual void StopSelection() {}
    virtual void SelectionFollowCursor() {}
    virtual Range SelectionRange() { return {{0, 0}, {0, 0}}; }

    virtual void SetCursorHint(size_t s_row, size_t s_col) = 0;

    // return kOk or kSelectionStarted
    virtual Result DoubleClick() { return kOk; };

    virtual void ScrollRows(int64_t count) = 0;
    virtual void ScrollCols(int64_t count) = 0;

    virtual void CursorGoUp(size_t count) = 0;
    virtual void CursorGoDown(size_t count) = 0;

    virtual void CursorGoHalfPageUp(size_t count) = 0;
    virtual void CursorGoHalfPageDown(size_t count) = 0;

    virtual void CursorGoPageUp(size_t count) = 0;
    virtual void CursorGoPageDown(size_t count) = 0;

    virtual void SaveView() = 0;
    virtual void RestoreView() = 0;

    virtual void BuildSearchContext(const std::string& pattern,
                                    const Range* range) = 0;
    virtual const std::string& GetSearchPattern() = 0;
    virtual SearchState CursorGoSearchResult(bool next, size_t count,
                                             bool keep_current_if_one) = 0;
    // return true if truly move view, else return false
    virtual bool ViewGoSearchResult(bool next, size_t count,
                                    bool keep_current_if_one) = 0;
};

}  // namespace charxed
