#include "status_line.h"

#include "buffer.h"
#include "cursor.h"
#include "draw.h"
#include "filetype.h"
#include "options.h"
#include "text_window.h"

namespace charxed {

StatusLine::StatusLine(Cursor* cursor, GlobalOpts* global_opts, Mode* mode,
                       Context* context)
    : cursor_(cursor),
      global_opts_(global_opts),
      mode_(mode),
      context_(context) {}

void StatusLine::Draw() {
    ThemeType t = kStatusLine;

    auto theme = global_opts_->GetOpt<Theme>(kOptTheme);

    left_str_.clear();
    right_str_.clear();
    switch (*context_) {
        case Context::kEditor: {
            Buffer* b;
            b = cursor_->t_win->area_.buffer_;
            fmt::format_to(std::back_inserter(left_str_),
                           "{:<" CHX_MODE_WIDTH "} {}{}",
                           kModeString[static_cast<int>(*mode_)], b->Name(),
                           kBufferStateString[static_cast<int>(b->state())]);
            int64_t line, character_in_line;
            if (IsPeel(*mode_)) {
                line = cursor_->t_win->area_.b_view_->cursor_state.pos.line;
                character_in_line = cursor_->t_win->area_.b_view_->cursor_state
                                        .character_in_line_;
            } else {
                line = cursor_->pos.line;
                character_in_line = cursor_->character_in_line;
            }

            fmt::format_to(
                std::back_inserter(right_str_), "  {},{}  {:>2}%  {}  {}{}  {}",
                line + 1, character_in_line + 1,
                100 * (line + 1) / b->LineCnt(),
                FiletypeUserStrRep(b->filetype()),
                b->opts().GetOpt<bool>(kOptTabSpace) ? "Sp" : "Tb",
                b->opts().GetOpt<int64_t>(kOptTabStop), b->eol_seq());
            break;
        }
        case Context::kExplorer: {
            fmt::format_to(
                std::back_inserter(left_str_), "{:<" CHX_MODE_WIDTH "} {}",
                kModeString[static_cast<int>(*mode_)], Path::GetCwd());
            fmt::format_to(std::back_inserter(right_str_), "  {}",
                           cursor_->pos.line);
            break;
        }
        default:
            break;
    }

    DrawLine(*term_, left_str_, {0, 0}, 0, width_, row_, 0, nullptr, theme,
             theme[t], left_str_.size(), 0, false, true);

    // all is ascii character, so str len == width
    term_->Print(width_ - right_str_.length(), row_, theme[t],
                 right_str_.c_str());
}

}  // namespace charxed
