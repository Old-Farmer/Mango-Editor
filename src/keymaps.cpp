#include "editor.h"
#include "subprocess.h"

namespace charxed {

#define CHX_KEYMAP keymap_manager_.AddKeyseq
void Editor::InitKeymaps() {
    // Navigation
    CHX_KEYMAP("h", {[this] { cursor_.t_win->CursorGoLeft(Count()); }});
    CHX_KEYMAP("h", {[this] { peel_->CursorGoLeft(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    // TODO: incapsulate in TextWindow ?
    CHX_KEYMAP("h", {[this] {
                   cursor_.t_win->area_
                       .SelectToWithCount<&TextArea::CursorGoLeftState>(
                           OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("l", {[this] { cursor_.t_win->CursorGoRight(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("l", {[this] { peel_->CursorGoRight(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("l", {[this] {
                   cursor_.t_win->area_
                       .SelectToWithCount<&TextArea::CursorGoRightState>(
                           OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("b", {[this] { cursor_.t_win->CursorGoWordBegin(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("b", {[this] { peel_->CursorGoPrevWordBegin(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP(
        "b", {[this] {
            cursor_.t_win->area_
                .SelectToWithCount<&TextArea::CursorGoPrevWordBeginState>(
                    OpPendingCount());
        }},
        {Mode::kOperatorPending});
    CHX_KEYMAP("e", {[this] { cursor_.t_win->CursorGoNextWordEnd(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("e", {[this] { peel_->CursorGoNextWordEnd(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("e", {[this] {
                   cursor_.t_win->area_
                       .SelectToWithCount<&TextArea::CursorGoNextWordEndState>(
                           OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("w", {[this] { cursor_.t_win->CursorGoNextWordBegin(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("w", {[this] { peel_->CursorGoNextWordBegin(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP(
        "w", {[this] {
            cursor_.t_win->area_
                .SelectToWithCount<&TextArea::CursorGoNextWordBeginState>(
                    OpPendingCount());
        }},
        {Mode::kOperatorPending});
    CHX_KEYMAP("k", {[this] { cursor_.focused->CursorGoUp(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("k", {[this] { peel_->CursorGoUp(Count()); }}, {Mode::kPeelShow},
               {CHX_ALL_CONTEXTS});
    // TODO: what about wrap?
    CHX_KEYMAP("k", {[this] {
                   cursor_.t_win->area_.SelectPrevLines(OpPendingCount() + 1);
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("j", {[this] { cursor_.focused->CursorGoDown(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("j", {[this] { peel_->CursorGoDown(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    // TODO: what about wrap?
    CHX_KEYMAP("j", {[this] {
                   cursor_.t_win->area_.SelectNextLines(OpPendingCount() + 1);
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("0", {[this] { cursor_.t_win->CursorGoHome(); }});
    CHX_KEYMAP("0", {[this] { peel_->CursorGoHome(); }}, {Mode::kPeelShow},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP(
        "0", {[this] {
            cursor_.t_win->area_.SelectTo<&TextArea::CursorGoHomeState>();
        }},
        {Mode::kOperatorPending});
    CHX_KEYMAP("^", {[this] { cursor_.t_win->CursorGoFirstNonBlank(); }});
    CHX_KEYMAP("^", {[this] {
                   cursor_.t_win->area_
                       .SelectTo<&TextArea::CursorGoFirstNonBlankState>();
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("$", {[this] { cursor_.t_win->CursorGoEnd(); }});
    CHX_KEYMAP("$", {[this] { peel_->CursorGoEnd(); }}, {Mode::kPeelShow},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("$", {[this] {
                   cursor_.t_win->area_.SelectTo<&TextArea::CursorGoEndState>();
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("%", {[this] {
                   if (count_ == 0)
                       cursor_.t_win->CursorGoBracket();
                   else if (Count() <= 100)
                       cursor_.t_win->CursorGoLine(
                           (cursor_.t_win->area_.buffer_->LineCnt() - 1) *
                           Count() / 100);
               }});
    CHX_KEYMAP(
        "%", {[this] {
            if (count_ == 0)
                peel_->CursorGoBracket();
            else if (Count() <= 100)
                peel_->CursorGoLine((peel_->area_.buffer_->LineCnt() - 1) *
                                    Count() / 100);
        }},
        {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP(
        "%", {[this] {
            if (count_ == 0 && op_pending_stored_count_ == 0)
                cursor_.t_win->area_.SelectTo<&TextArea::CursorGoBracketState>(
                    true);
        }},
        {Mode::kOperatorPending});
    CHX_KEYMAP("<c-f>",
               {[this] { cursor_.focused->CursorGoPageDown(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("<c-f>", {[this] { peel_->CursorGoPageDown(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-b>", {[this] { cursor_.focused->CursorGoPageUp(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-b>", {[this] { peel_->CursorGoPageUp(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-d>",
               {[this] { cursor_.focused->CursorGoHalfPageDown(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-d>", {[this] { peel_->CursorGoHalfPageDown(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-u>",
               {[this] { cursor_.focused->CursorGoHalfPageUp(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-u>", {[this] { peel_->CursorGoHalfPageUp(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-o>", {[this] { cursor_.t_win->JumpBackward(); }},
               {Mode::kNormal});
    CHX_KEYMAP("<c-i>", {[this] { cursor_.t_win->JumpForward(); }},
               {Mode::kNormal});
    CHX_KEYMAP("G", {[this] {
                   cursor_.t_win->CursorGoLine(
                       count_ == 0 ? cursor_.t_win->area_.buffer_->LineCnt() - 1
                                   : Count());
               }});
    CHX_KEYMAP("G", {[this] {
                   cursor_.t_win->area_.SelectLinesToLine(
                       count_ == 0 && op_pending_stored_count_ == 0
                           ? cursor_.t_win->area_.buffer_->LineCnt() - 1
                           : OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("gg", {[this] { cursor_.t_win->CursorGoLine(0); }});
    CHX_KEYMAP("gg", {[this] { cursor_.t_win->area_.SelectLinesToLine(0); }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("gf", {[this] { cursor_.t_win->GotoFile(); }});
    // TODO: op pending?
    CHX_KEYMAP("f<any-cp>", {[this] {
                   find_forward_ = true;
                   c_to_find_ = CombineACharacterFromInput(
                       term_.EventKeyInfo().codepoint);
                   cursor_.t_win->FindNextCharacterAndCursorGoInCurrentLine(
                       c_to_find_);
               }});
    CHX_KEYMAP("F<any-cp>", {[this] {
                   find_forward_ = false;
                   c_to_find_ = CombineACharacterFromInput(
                       term_.EventKeyInfo().codepoint);
                   cursor_.t_win->FindPrevCharacterAndCursorGoInCurrentLine(
                       c_to_find_);
               }});
    CHX_KEYMAP(";", {[this] {
                   if (find_forward_) {
                       cursor_.t_win->FindNextCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   } else {
                       cursor_.t_win->FindPrevCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   }
               }});
    CHX_KEYMAP(",", {[this] {
                   if (find_forward_) {
                       cursor_.t_win->FindPrevCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   } else {
                       cursor_.t_win->FindNextCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   }
               }});

    // Buffer manangement
    CHX_KEYMAP("]b", {[this] { cursor_.t_win->NextBuffer(); }},
               {Mode::kNormal});
    CHX_KEYMAP("[b", {[this] { cursor_.t_win->PrevBuffer(); }},
               {Mode::kNormal});

    // esc
    CHX_KEYMAP("<esc>", {[this] {
                   if (IsPeel(mode_)) {
                       NotifyUser("");
                   }
                   ExitFromMode();
               }},
               {CHX_ALL_MODES}, {CHX_ALL_CONTEXTS});

    // command & search
    CHX_KEYMAP("/", {[this] {
                   search_foward_ = true;
                   GotoPeel(Mode::kPeelSearch);
               }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("?", {[this] {
                   search_foward_ = false;
                   GotoPeel(Mode::kPeelSearch);
               }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("N",
               {[this] { CursorGoSearch(!search_foward_, Count(), false); }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("n",
               {[this] { CursorGoSearch(search_foward_, Count(), false); }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP(":", {[this] { GotoPeel(Mode::kPeelCommand); }}, {Mode::kNormal},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<enter>", {[this] { GotoPeel(Mode::kPeelShow); }},
               {Mode::kNormal});
    CHX_KEYMAP("<c-r>", {[this] {
                   peel_->Paste();
                   layout_manager_->ArrangeLayout();
               }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<bs>", {[this] {
                   if (peel_->DeleteCharacterBeforeCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           mode_ == Mode::kPeelCommand
                               ? EditorEvent::kCommandCharEdit
                               : EditorEvent::kSearchCharEdit,
                           nullptr);
                       layout_manager_->ArrangeLayout();
                   }
               }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-w>", {[this] {
                   peel_->DeleteWordBeforeCursor();
                   editor_event_manager_.EmitEvent(
                       mode_ == Mode::kPeelCommand
                           ? EditorEvent::kCommandCharEdit
                           : EditorEvent::kSearchCharEdit,
                       nullptr);
                   layout_manager_->ArrangeLayout();
               }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<enter>", {[this] {
                   CommandHitEnter();
                   multirow_peel_keep_ = true;
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<enter>", {[this] {
                   SearchHitEnter();
                   multirow_peel_keep_ = true;
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<left>", {[this] { peel_->CursorGoLeft(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<right>", {[this] { peel_->CursorGoRight(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-left>", {[this] { peel_->CursorGoPrevWordBegin(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-right>", {[this] { peel_->CursorGoNextWordEnd(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<home>", {[this] { peel_->CursorGoHome(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<end>", {[this] { peel_->CursorGoEnd(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});

    // cmp & history
    CHX_KEYMAP("<c-space>", {[this] { TriggerCompletion(false); }},
               {Mode::kInsert, Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-c>", {[this] { TriggerCompletion(false); }},
               {Mode::kInsert, Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<tab>", {[this] {
                   if (CompletionTriggered()) {
                       if (completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
                           kRetriggerCmp) {
                           completer_ = nullptr;
                           TriggerCompletion(true);
                           layout_manager_->ArrangeLayout();
                       } else {
                           completer_ = nullptr;
                       }
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<tab>", {[this] {
                   if (CompletionTriggered()) {
                       if (completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
                           kRetriggerCmp) {
                           completer_ = nullptr;
                           TriggerCompletion(true);
                           layout_manager_->ArrangeLayout();
                       } else {
                           completer_ = nullptr;
                       }
                   } else {
                       cursor_.t_win->TabAtCursor();
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-n>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectNext(1);
                       show_cmp_menu_ = true;
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-n>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectNext(1);
                       show_cmp_menu_ = true;
                   } else {
                       peel_->NextHistoryItem(MangoPeel::HistoryType::kCmd);
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-n>", {[this] {
                   peel_->NextHistoryItem(MangoPeel::HistoryType::kSearch);
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-p>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectPrev(1);
                       show_cmp_menu_ = true;
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-p>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectPrev(1);
                       show_cmp_menu_ = true;
                   } else {
                       peel_->PrevHistoryItem(MangoPeel::HistoryType::kCmd);
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-p>", {[this] {
                   peel_->PrevHistoryItem(MangoPeel::HistoryType::kSearch);
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});

    // Edit
    CHX_KEYMAP("<c-s>", {[this] { SaveCurrentBuffer(); }},
               {CHX_DEFAULT_MODES, Mode::kInsert});
    CHX_KEYMAP("<bs>", {[this] {
                   if (cursor_.t_win->DeleteAtCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kEditCharEdit, nullptr);
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-w>", {[this] { cursor_.t_win->DeleteWordBeforeCursor(); }},
               {Mode::kInsert});
    CHX_KEYMAP("<enter>", {[this] { cursor_.t_win->AddStringAtCursor("\n"); }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-r>", {[this] { cursor_.t_win->Paste(1, false); }},
               {Mode::kInsert});
    CHX_KEYMAP("i", {[this] { GotoMode(Mode::kInsert); }}, {Mode::kNormal});
    CHX_KEYMAP("I", {[this] {
                   cursor_.t_win->CursorGoFirstNonBlank();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});  // TODO it
    CHX_KEYMAP("a", {[this] {
                   cursor_.t_win->CursorGoRight(Count());
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("A", {[this] {
                   cursor_.t_win->CursorGoEnd();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("o", {[this] {
                   cursor_.t_win->NewLineUnderCursorline();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("O", {[this] {
                   cursor_.t_win->NewLineAboveCursorline();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("u", {[this] { cursor_.t_win->Undo(); }}, {Mode::kNormal});
    CHX_KEYMAP("<c-r>", {[this] { cursor_.t_win->Redo(); }}, {Mode::kNormal});
    CHX_KEYMAP("y", {[this] {
                   cursor_.t_win->Copy();
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("Y", {[this] {
                   // y$
                   cursor_.t_win->area_.SelectTo<&TextArea::CursorGoEndState>();
                   if (cursor_.t_win->IsSelectionActive())
                       cursor_.t_win->Copy();
               }},
               {Mode::kNormal});
    CHX_KEYMAP("y", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kYank;
               }},
               {Mode::kNormal});
    CHX_KEYMAP("p", {[this] {
                   cursor_.t_win->Paste(Count(), true);
                   ExitFromMode();
               }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("P", {[this] {
                   cursor_.t_win->Paste(Count(), false);
                   ExitFromMode();
               }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("d", {[this] {
                   cursor_.t_win->Cut();
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("D", {[this] {
                   // d$
                   cursor_.t_win->area_.SelectTo<&TextArea::CursorGoEndState>();
                   if (cursor_.t_win->IsSelectionActive())
                       cursor_.t_win->DeleteSelection();
               }},
               {Mode::kNormal});
    CHX_KEYMAP("d", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kDelete;
               }},
               {Mode::kNormal});
    CHX_KEYMAP("\\>", {[this] {
                   cursor_.t_win->IndentSelection(Count());
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("\\>", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kIndent;
               }},
               {Mode::kNormal});
    CHX_KEYMAP("\\<", {[this] {
                   cursor_.t_win->UnindentSelection(Count());
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("\\<", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kUnindent;
               }},
               {Mode::kNormal});

    // A inner format, not exposed
    CHX_KEYMAP(
        "<space>f", {[this] {
            auto& path = cursor_.t_win->area_.buffer_->path();
            auto filetype = cursor_.t_win->area_.buffer_->filetype();
            if (path.Empty() || filetype != FileType::kC ||
                filetype != FileType::kCpp) {
                return;
            }
            try {
                const char* const argv[] = {"clang-format", "--assume-filename",
                                            path.AbsolutePath().c_str(),
                                            nullptr};
                Buffer* b = cursor_.t_win->area_.buffer_;
                int exit_code;
                std::string stdout;
                std::string stdin =
                    TextTree::TextView{b->Begin(), b->End()}.ToString();
                Result res = Exec(argv, &stdin, &stdout, nullptr, exit_code);
                if (res != kOk || exit_code != 0 || !CheckUtf8Valid(stdout)) {
                    return;
                }
                if (stdin == stdout) {
                    return;
                }
                cursor_.t_win->area_.b_view_->make_cursor_visible = true;
                Pos pos = FixCursorPos(cursor_.pos, stdout);
                // TODO: diff
                cursor_.t_win->area_.Replace(
                    {{0, 0},
                     {b->LineCnt() - 1,
                      b->GetLineView(b->LineCnt() - 1).Size()}},
                    stdout, &pos);
            } catch (OSException& e) {
                // TODO: notify
            }
        }},
        {Mode::kNormal});

    // op-pending/selection text object
    CHX_KEYMAP("d", {[this] {
                   if (pending_operator_ != Operator::kDelete) {
                       return;
                   }
                   cursor_.t_win->area_.SelectNextLines(OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("y", {[this] {
                   if (pending_operator_ != Operator::kYank) {
                       return;
                   }
                   cursor_.t_win->area_.SelectNextLines(OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("\\>", {[this] {
                   if (pending_operator_ != Operator::kIndent) {
                       return;
                   }
                   cursor_.t_win->area_.SelectNextLines(OpPendingCount());
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("\\<", {[this] {
                   if (pending_operator_ != Operator::kUnindent) {
                       return;
                   }
                   cursor_.t_win->area_.SelectNextLines(OpPendingCount());
               }},
               {Mode::kOperatorPending});
    for (char open : PairOpens()) {
        std::string a = "a";
        std::string i = "i";
        char close = IsPairOpen(open).second;
        std::vector<char> pair_c = {open};
        if (close != open) {
            pair_c.push_back(close);
        }
        for (char p : pair_c) {
            CHX_KEYMAP(a + p, {[this, open] {
                           cursor_.t_win->area_.SelectPair(open, false);
                       }},
                       {Mode::kOperatorPending, Mode::kSelect});
            CHX_KEYMAP(a + p, {[this, open] {
                           if (cursor_.t_win->area_.SelectPair(open, false))
                               mode_ = Mode::kSelect;
                       }},
                       {Mode::kSelectLine});
            CHX_KEYMAP(i + p, {[this, open] {
                           cursor_.t_win->area_.SelectPair(open, true);
                       }},
                       {Mode::kOperatorPending, Mode::kSelect});
            CHX_KEYMAP(i + p, {[this, open] {
                           if (cursor_.t_win->area_.SelectPair(open, true))
                               mode_ = Mode::kSelect;
                       }},
                       {Mode::kSelectLine});
        }
    }

    // Selection
    CHX_KEYMAP("s", {[this] {
                   cursor_.t_win->area_.StartLineSelection(cursor_.pos);
                   GotoMode(Mode::kSelectLine);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("S", {[this] {
                   cursor_.t_win->area_.StartSelection(cursor_.pos);
                   GotoMode(Mode::kSelect);
               }},
               {Mode::kNormal});

    // Explorer
    CHX_KEYMAP("<space>e", {[this] { OpenExplorer(); }}, {Mode::kNormal});
    CHX_KEYMAP("q", {[this] { QuitExplorer(); }}, {Mode::kNormal},
               {Context::kExplorer});
    CHX_KEYMAP(
        "<enter>", {[this] {
            if (peel_->area_.height_ != 1) {
                GotoPeel(Mode::kPeelShow);
                return;
            }
            try {
                explorer_->EnterCurrentEntry();
            } catch (FSException& e) {
                NotifyUser(fmt::format("expand dir entry error: {}", e.what()));
            }
        }},
        {Mode::kNormal}, {Context::kExplorer});
}

}  // namespace charxed
