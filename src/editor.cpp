#include "editor.h"

#include "character.h"
#include "clipboard.h"
#include "constants.h"
#include "fs.h"
#include "inttypes.h"  // IWYU pragma: keep
#include "options.h"
#include "term.h"

// TODO: show sth. to users if modify the readonly buffer.

namespace mango {

static constexpr int kScreenMinWidth = 10;
static constexpr int kScreenMinHeight = 3;

void Editor::Init(std::unique_ptr<GlobalOpts> global_opts,
                  std::unique_ptr<InitOpts> init_opts) {
    global_opts_ = std::move(global_opts);

    loop_ = std::make_unique<EventLoop>(global_opts_.get());

    term_.Init(global_opts_.get());

    buffer_manager_ = std::make_unique<BufferManager>(&editor_event_manager_);
    clipboard_ = ClipBoard::CreateClipBoard(true);

    // Component
    status_line_ =
        std::make_unique<StatusLine>(&cursor_, global_opts_.get(), &mode_);
    peel_ = std::make_unique<MangoPeel>(
        &cursor_, global_opts_.get(), clipboard_.get(), buffer_manager_.get());
    cmp_menu_ = std::make_unique<CmpMenu>(&cursor_, global_opts_.get());

    syntax_parser_ = std::make_unique<SyntaxParser>(global_opts_.get());

    // Create all buffers
    for (const char* path : init_opts->begin_files) {
        buffer_manager_->AddBuffer(
            Buffer(global_opts_.get(), std::string(path)));
    }

    // Create the first window.
    // If no buffer then create one no file backup buffer.
    Buffer* buf;
    if (buffer_manager_->Begin() != nullptr) {
        buf = buffer_manager_->Begin();
    } else {
        buf = buffer_manager_->AddBuffer({global_opts_.get()});
    }
    MGO_LOG_DEBUG("buffer {}", buf->Name());
    window_ = std::make_unique<Window>(&cursor_, global_opts_.get(),
                                       syntax_parser_.get(), clipboard_.get(),
                                       buffer_manager_.get());

    // Set Cursor in the first window
    cursor_.in_window = window_.get();
    window_->AttachBuffer(buf);

    // Layout
    layout_manager_ = std::make_unique<LayoutManager>(
        window_.get(), status_line_.get(), peel_.get());
    layout_manager_->ArrangeLayout();

    // Keymaps & commands
    term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
    InitKeymaps();
    InitCommands();

    if (!global_opts_->IsUserConfigValid()) {
        NotifyUser(
            "Error in your config file! Default config loaded. Please "
            "check your config." +
            global_opts_->GetUserConfigErrorReportStrAndReleaseIt());
    }

    RegisterEditorEventHandlers();
}

void Editor::RegisterEditorEventHandlers() {
    editor_event_manager_.AddHandler(
        EditorEvent::kBufferRemoved, [this](void* arg) {
            auto buffer = reinterpret_cast<const Buffer*>(arg);
            window_->OnBufferDelete(buffer);
            syntax_parser_->OnBufferDelete(buffer);
        });
    editor_event_manager_.AddHandler(EditorEvent::kEditCharEdit,
                                     [this](void* arg) {
                                         (void)arg;
                                         StartAutoCompletionTimer();
                                     });
    editor_event_manager_.AddHandler(EditorEvent::kCommandCharEdit,
                                     [this](void* arg) {
                                         (void)arg;
                                         StartAutoCompletionTimer();
                                     });
    editor_event_manager_.AddHandler(EditorEvent::kSearchCharEdit,
                                     [this](void* arg) {
                                         (void)arg;
                                         StartSearchOnTypeTimer();
                                     });
}

void Editor::Loop() {
    bool in_bracketed_paste = false;
    std::string bracketed_paste_buffer;

    auto before_poll = [this] {
        // When the screen is too small, rendering is hard to cope with,
        // so we just don't do anything, swallow all events except resize
        // until the screen is bigger again.
        while (term_.Height() < kScreenMinHeight ||
               term_.Width() < kScreenMinWidth) {
            if (term_.Poll(-1)) {
                if (term_.WhatEvent() == Terminal::EventType::kResize) {
                    HandleResize();
                }
            }
        }

        PreProcess();
        Draw();
    };

    auto term_handler = [this, &in_bracketed_paste,
                         &bracketed_paste_buffer](Event e) {
        (void)e;
        MGO_ASSERT(e & kEventRead);
        if (e & (kEventClose | kEventError)) {
            throw TermException("{}", "Poll event close or event error");
        }

        // We use a while to eat all events.
        // Usually, there will be only one event.
        // Bracketed Paste and multi-codepoint grapheme will lead to mult
        // events; Otherwise, multi events means we meet a slow machine, bad
        // network or a bad routine executes too long. I think we should handle
        // it.
        while (term_.Poll(0)) {
            show_cmp_menu_ = false;
            if (autocmp_trigger_timer_ &&
                autocmp_trigger_timer_->IsTimingOn()) {
                loop_->timer_manager_.StopTimer(autocmp_trigger_timer_.get());
                MGO_ASSERT(!autocmp_trigger_timer_->IsTimingOn());
            }

            multirow_peel_keep_ = false;

            // Handle it and do sth
            switch (term_.WhatEvent()) {
                case Terminal::EventType::kKey: {
                    if (in_bracketed_paste) {
                        HandleBracketedPaste(bracketed_paste_buffer);
                    } else {
                        HandleKey();
                    }
                    break;
                }
                case Terminal::EventType::kMouse: {
                    HandleMouse();
                    break;
                }
                case Terminal::EventType::kResize: {
                    HandleResize();
                    break;
                }
                case Terminal::EventType::kBracketedPasteOpen: {
                    in_bracketed_paste = true;
                    break;
                }
                case Terminal::EventType::kBracketedPasteClose: {
                    in_bracketed_paste = false;
                    if (IsPeel(mode_)) {
                        peel_->AddStringAtCursor(
                            std::move(bracketed_paste_buffer));
                    } else {
                        cursor_.in_window->AddStringAtCursor(
                            std::move(bracketed_paste_buffer));
                    }
                    bracketed_paste_buffer = "";
                    break;
                }
            }
        }

        // If autocmp trigger timer has started,
        // don't cancel it to avoid flash of cmp menu.
        if (!show_cmp_menu_ &&
            (autocmp_trigger_timer_ && !autocmp_trigger_timer_->IsTimingOn())) {
            CancellCompletion();
        }

        // Hide peel multi row content if user don't in the peel.
        if (!multirow_peel_keep_ && peel_->area_.height_ >= 2 &&
            !IsPeel(mode_)) {
            std::string output_hidden_hint =
                fmt::format("[Collapse {} rows...]", peel_->area_.height_);
            if (output_hidden_hint.size() > term_.Width()) {
                output_hidden_hint.resize(term_.Width());
            }
            NotifyUser(output_hidden_hint);
        }
    };

    EventInfo term_tty;
    EventInfo term_resize;
    term_.GetFDs(term_tty.fd, term_resize.fd);
    term_tty.Interesting_events |= kEventRead;
    term_resize.Interesting_events |= kEventRead;
    term_tty.handler = term_handler;
    term_resize.handler = std::move(term_handler);

    loop_->BeforePoll(std::move(before_poll));
    loop_->AddEventHandler(std::move(term_tty));
    loop_->AddEventHandler(std::move(term_resize));

    loop_->Loop();
}

#define MGO_KEYMAP keymap_manager_.AddKeyseq

void Editor::InitKeymaps() {
    // Navigation
    MGO_KEYMAP("h", {[this] { cursor_.in_window->CursorGoLeft(Count()); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("l", {[this] { cursor_.in_window->CursorGoRight(Count()); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("b", {[this] { cursor_.in_window->CursorGoWordBegin(Count()); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("e", {[this] {
                   cursor_.in_window->CursorGoNextWordEnd(Count(), false);
               }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("w",
               {[this] { cursor_.in_window->CursorGoNextWordBegin(Count()); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("k", {[this] { cursor_.in_window->CursorGoUp(Count()); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("j", {[this] { cursor_.in_window->CursorGoDown(Count()); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("0", {[this] { cursor_.in_window->CursorGoHome(); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("$", {[this] { cursor_.in_window->CursorGoEnd(); }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP(
        "<c-f>", {[this] {
            cursor_.in_window->CursorGoDown(cursor_.in_window->area_.height_);
        }},
        {MGO_DEFAULT_MODES});
    MGO_KEYMAP(
        "<c-b>", {[this] {
            cursor_.in_window->CursorGoUp(cursor_.in_window->area_.height_);
        }},
        {MGO_DEFAULT_MODES});
    MGO_KEYMAP("<c-d>", {[this] {
                   cursor_.in_window->CursorGoDown(
                       cursor_.in_window->area_.height_ / 2);
               }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP(
        "<c-u>", {[this] {
            cursor_.in_window->CursorGoUp(cursor_.in_window->area_.height_ / 2);
        }},
        {MGO_DEFAULT_MODES});
    MGO_KEYMAP("<c-o>", {[this] { cursor_.in_window->JumpBackward(); }},
               {Mode::kNormal});
    MGO_KEYMAP("<c-i>", {[this] { cursor_.in_window->JumpForward(); }},
               {Mode::kNormal});
    MGO_KEYMAP("G", {[this] {
                   size_t l;
                   if (count_ == 0) {
                       l = cursor_.in_window->area_.buffer_->LineCnt() - 1;
                   } else {
                       l = Count();
                   }
                   cursor_.in_window->CursorGoLine(l);
               }},
               {MGO_DEFAULT_MODES});
    MGO_KEYMAP("gg", {[this] { cursor_.in_window->CursorGoLine(0); }},
               {MGO_DEFAULT_MODES});

    // Buffer manangement
    MGO_KEYMAP("]b", {[this] { cursor_.in_window->NextBuffer(); }},
               {Mode::kNormal});
    MGO_KEYMAP("[b", {[this] { cursor_.in_window->PrevBuffer(); }},
               {Mode::kNormal});

    // esc
    MGO_KEYMAP("<esc>", {[this] {
                   if (IsPeel(mode_)) {
                       peel_->ShowContent("");
                   }
                   ExitFromMode();
               }},
               {MGO_ALL_MODES});

    // command & search
    MGO_KEYMAP("/", {[this] {
                   GotoPeel(Mode::kPeelSearch);
                   search_foward_ = true;
               }},
               {Mode::kNormal});
    MGO_KEYMAP("?", {[this] {
                   GotoPeel(Mode::kPeelSearch);
                   search_foward_ = false;
               }},
               {Mode::kNormal});
    MGO_KEYMAP("N",
               {[this] { CursorGoSearch(!search_foward_, Count(), false); }},
               {Mode::kNormal});
    MGO_KEYMAP("n",
               {[this] { CursorGoSearch(search_foward_, Count(), false); }},
               {Mode::kNormal});
    MGO_KEYMAP(":", {[this] { GotoPeel(); }}, {Mode::kNormal});
    MGO_KEYMAP("<enter>", {[this] { GotoPeel(Mode::kPeelShow); }},
               {Mode::kNormal});
    MGO_KEYMAP("<c-r>\"", {[this] { peel_->Paste(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<bs>", {[this] {
                   if (peel_->DeleteCharacterBeforeCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kCommandCharEdit, nullptr);
                   }
               }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<bs>", {[this] {
                   if (peel_->DeleteCharacterBeforeCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kSearchCharEdit, nullptr);
                   }
               }},
               {Mode::kPeelSearch});
    MGO_KEYMAP("<c-w>", {[this] { peel_->DeleteWordBeforeCursor(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<enter>", {[this] { CommandHitEnter(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<enter>", {[this] { SearchHitEnter(); }}, {Mode::kPeelSearch});
    MGO_KEYMAP("<left>", {[this] { peel_->CursorGoLeft(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<right>", {[this] { peel_->CursorGoRight(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<c-left>", {[this] { peel_->CursorGoPrevWordBegin(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<c-right>",
               {[this] { peel_->CursorGoNextWordEnd(Count(), true); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<home>", {[this] { peel_->CursorGoHome(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});
    MGO_KEYMAP("<end>", {[this] { peel_->CursorGoEnd(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch});

    // cmp
    MGO_KEYMAP("<c-space>", {[this] { TriggerCompletion(false); }},
               {Mode::kInsert, Mode::kPeelCommand});
    MGO_KEYMAP("<c-c>", {[this] { TriggerCompletion(false); }},
               {Mode::kInsert, Mode::kPeelCommand});
    MGO_KEYMAP("<tab>", {[this] {
                   if (CompletionTriggered()) {
                       if (completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
                           kRetriggerCmp) {
                           completer_ = nullptr;
                           TriggerCompletion(true);
                       } else {
                           completer_ = nullptr;
                       }
                   } else {
                       if (cursor_.in_window) cursor_.in_window->TabAtCursor();
                   }
               }},
               {Mode::kInsert, Mode::kPeelCommand});
    MGO_KEYMAP("<c-n>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectNext(1);
                       show_cmp_menu_ = true;
                   }
               }},
               {Mode::kInsert, Mode::kPeelCommand});
    MGO_KEYMAP("<c-p>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectNext(1);
                       show_cmp_menu_ = true;
                   }
               }},
               {Mode::kInsert, Mode::kPeelCommand});

    // Edit
    MGO_KEYMAP("<bs>", {[this] {
                   if (cursor_.in_window->DeleteAtCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kEditCharEdit, nullptr);
                   }
               }},
               {Mode::kInsert});
    MGO_KEYMAP("<c-w>",
               {[this] { cursor_.in_window->DeleteWordBeforeCursor(); }},
               {Mode::kInsert});
    MGO_KEYMAP("<enter>",
               {[this] { cursor_.in_window->AddStringAtCursor("\n"); }},
               {Mode::kInsert});
    MGO_KEYMAP("i", {[this] { GotoMode(Mode::kInsert); }}, {Mode::kNormal});
    MGO_KEYMAP("I", {[this] {
                   cursor_.in_window->CursorGoFirstNonBlank();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});  // TODO it
    MGO_KEYMAP("a", {[this] {
                   cursor_.in_window->CursorGoRight(Count());
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    MGO_KEYMAP("A", {[this] {
                   cursor_.in_window->CursorGoEnd();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    MGO_KEYMAP("o", {[this] {
                   cursor_.in_window->NewLineUnderCursorline();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    MGO_KEYMAP("O", {[this] {
                   cursor_.in_window->NewLineAboveCursorline();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    MGO_KEYMAP("u", {[this] { cursor_.in_window->Undo(); }}, {Mode::kNormal});
    MGO_KEYMAP("<c-r>", {[this] { cursor_.in_window->Redo(); }},
               {Mode::kNormal});
    MGO_KEYMAP("y", {[this] {
                   cursor_.in_window->Copy();
                   ExitFromMode();
               }},
               {MGO_VISUAL_MODES});
    MGO_KEYMAP("y", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kYank;
               }},
               {Mode::kNormal});
    MGO_KEYMAP("p", {[this] {
                   cursor_.in_window->Paste(Count());
                   ExitFromMode();
               }},
               {Mode::kNormal});
    // TODO: p in visual mode
    MGO_KEYMAP("d", {[this] {
                   cursor_.in_window->Cut();
                   ExitFromMode();
               }},
               {MGO_VISUAL_MODES});
    MGO_KEYMAP("d", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kDelete;
               }},
               {Mode::kNormal});

    // Operator Pending motion / text object
    MGO_KEYMAP("d", {[this] {
                   // TODO
                   ExitFromMode();
               }},
               {Mode::kOperatorPending});
    MGO_KEYMAP("y", {[this] {
                   // TODO
                   ExitFromMode();
               }},
               {Mode::kOperatorPending});

    // Selection
    MGO_KEYMAP("v", {[this] {
                   cursor_.in_window->area_.StartSelection(cursor_.pos);
                   GotoMode(Mode::kVisual);
               }},
               {Mode::kNormal});
    MGO_KEYMAP("V", {[this] {
                   cursor_.in_window->area_.StartLineSelection(cursor_.pos);
                   GotoMode(Mode::kVisualLine);
               }},
               {Mode::kNormal});
#define MGO_CMD command_manager_.AddCommand
}

void Editor::InitCommands() {
#define MGO_ENSURE_ARGEXITS(v) MGO_ASSERT(args[v].has_value())
    MGO_CMD({"quit", "q", "", {}, [this](CommandArgs args) {
                 (void)args;
                 Quit(false);
             }});
    MGO_CMD({"quit!", "q!", "", {}, [this](CommandArgs args) {
                 (void)args;
                 Quit(true);
             }});
    MGO_CMD({"help",
             "h",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 if (!args[0].has_value()) {
                     Help(kHelpDoc);
                 } else {
                     Help(std::get<std::string>(args[0].value()));
                 }
             },
             1,
             1});
    MGO_CMD({"write",
             "w",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 if (args[0].has_value()) {
                     Path p(std::get<std::string>(args[0].value()));
                     SaveCurrentBufferAs(p);
                 } else {
                     SaveCurrentBuffer();
                 }
             },
             1,
             1});
    MGO_CMD({"edit",
             "e",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 MGO_ENSURE_ARGEXITS(0);
                 const std::string& name_str =
                     std::get<std::string>(args[0].value());
                 Path p = Path(name_str);
                 Buffer* b = buffer_manager_->FindBuffer(p);
                 if (b) {
                     cursor_.in_window->AttachBuffer(b);
                     return;
                 }
                 cursor_.in_window->AttachBuffer(buffer_manager_->AddBuffer(
                     Buffer(global_opts_.get(), std::move(p))));
             },
             1});
    MGO_CMD({"buffer",
             "b",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 MGO_ENSURE_ARGEXITS(0);
                 const std::string& name_str =
                     std::get<std::string>(args[0].value());
                 Buffer* b = buffer_manager_->FindBuffer(name_str);
                 if (b) {
                     cursor_.in_window->AttachBuffer(b);
                 }
             },
             1});
    MGO_CMD({"bdelete", "bd", "", {}, [this](CommandArgs args) {
                 (void)args;
                 RemoveCurrentBuffer();
             }});
    MGO_CMD({"smile",
             "",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 (void)args;
                 NotifyUser(kSmile);
             },
             0,
             0});
#undef MGO_ENSURE_ARGEXITS
}

void Editor::PrintKey(const Terminal::KeyInfo& key_info) {
    bool ctrl = key_info.mod & Terminal::kCtrl;
    bool shift = key_info.mod & Terminal::kShift;
    bool alt = key_info.mod & Terminal::kAlt;
    bool motion = key_info.mod & Terminal::kMotion;
    (void)ctrl, (void)shift, (void)alt, (void)motion;
    char c[5];
    int len = UnicodeToUtf8(key_info.codepoint, c);
    c[len] = '\0';
    MGO_LOG_DEBUG(
        "ctrl {} shift {} alt {} motion {} special key {} codepoint "
        "\\U{:08X} char {}",
        ctrl, shift, alt, motion, static_cast<int>(key_info.special_key),
        key_info.codepoint, c);
}

void Editor::HandleBracketedPaste(std::string& bracketed_paste_buffer) {
    Terminal::KeyInfo key_info = term_.EventKeyInfo();

#ifndef NDEBUG
    if (global_opts_->GetOpt<bool>(kOptLogVerbose)) {
        PrintKey(key_info);
    }
#endif  // !NDEBUG

    if (key_info.IsSpecialKey() && key_info.mod == Terminal::kCtrl &&
        key_info.special_key == Terminal::SpecialKey::kEnter) {
        if (!IsPeel(mode_)) {
            bracketed_paste_buffer.push_back('\n');
        }
    } else if (key_info.IsSpecialKey()) {
        bracketed_paste_buffer.append(kReplacement);
        MGO_LOG_INFO("Unknown Special key in bracketed paste");
    } else {
        char c[5];
        int len = UnicodeToUtf8(key_info.codepoint, c);
        c[len] = '\0';
        MGO_ASSERT(len > 0);
        bracketed_paste_buffer.append(c);
    }
}

void Editor::HandleKey() {
    Terminal::KeyInfo key_info = term_.EventKeyInfo();

#ifndef NDEBUG
    if (global_opts_->GetOpt<bool>(kOptLogVerbose)) {
        PrintKey(key_info);
    }
#endif  // !NDEBUG

    // We treat one codepoint as a key event instead of one grapheme.
    // 1. It's a limitation on terminals now. We can't detect graphemes on
    // input event reliably, especially on ssh.
    // 2. Users can input single codepoints.
    // 3. Our keymaps only use special keys, or just ascii characters, which
    // users will be aware of.

    // If the editor is in count state, means user have already input a seq of
    // numbers, we calc the input here.
    if (input_state_ == InputState::kCount && !key_info.IsSpecialKey() &&
        key_info.codepoint >= '0' && key_info.codepoint <= '9') {
        count_ = count_ * 10 + key_info.codepoint - '0';
        return;
    }

    Result res = kKeyseqError;
    Keyseq* handler;
    if (key_info.IsSpecialKey() || key_info.codepoint <= CHAR_MAX) {
        res = keymap_manager_.FeedKey(key_info, handler);
    }
    if (res == kKeyseqDone) {
        handler->f();
        input_state_ = InputState::kNone;
        if (mode_ == Mode::kOperatorPending) {
            // key seq like 2d2l -> d4l
            op_pending_stored_count_ = count_;
        }
        count_ = 0;
        return;
    } else if (res == kKeyseqMatched) {
        // MGO_LOG_DEBUG("keymap matched");

        // Encounter a sequnce, we let multirow peel stay.
        if (!IsPeel(mode_) && peel_->area_.height_ > 1) {
            multirow_peel_keep_ = true;
        }
        return;
    }

    if (res == kKeyseqError) {
        // Pure codepoints that are not handled by the keymap manager.
        // Use single codepoint to edit buffers is quite safe here.

        // We may use a codepoint as grapheme when we meet some ascii
        // characters, like '(', '[' '{', because they're very very rare as a
        // part of multi-codepoint graphemes.
        if (key_info.IsSpecialKey()) {
            count_ = 0;
            input_state_ = InputState::kNone;
            return;
        }

        if (mode_ == Mode::kInsert || mode_ == Mode::kPeelCommand ||
            mode_ == Mode::kPeelSearch) {
            char c[5];
            int len = UnicodeToUtf8(key_info.codepoint, c);
            c[len] = '\0';
            MGO_ASSERT(len > 0);
            Result res;
            if (IsPeel(mode_)) {
                res = peel_->AddStringAtCursor(c);
            } else {
                res = cursor_.in_window->AddStringAtCursor(c);
            }
            if (res != kOk) {
                return;
            }
            EditorEvent ev;
            switch (mode_) {
                case Mode::kInsert:
                    ev = EditorEvent::kEditCharEdit;
                    break;
                case Mode::kPeelCommand:
                    ev = EditorEvent::kCommandCharEdit;
                    break;
                case Mode::kPeelSearch:
                    ev = EditorEvent::kSearchCharEdit;
                    break;
                default:
                    ev = EditorEvent::__kCount;
                    MGO_ASSERT("Can't reach here");
                    break;
            }
            editor_event_manager_.EmitEvent(ev, nullptr);
        } else if (key_info.codepoint >= '0' && key_info.codepoint <= '9' &&
                   input_state_ == InputState::kNone) {
            count_ = count_ * 10 + key_info.codepoint - '0';
            input_state_ = InputState::kCount;
        }
    }
}

void Editor::HandleLeftClick(int s_row, int s_col) {
    Window* win = LocateWindow(s_col, s_row);
    Window* prev_win = cursor_.in_window;
    Pos prev_pos = cursor_.pos;  // TODO: check this logic.
    if (win) {
        cursor_.in_window = win;
        win->SetCursorHint(s_row, s_col);
    }

    if (mouse_.state == MouseState::kReleased) {
        if (!win) {
            return;
        }
        if (cursor_.in_window->area_.IsSelectionActive()) {
            cursor_.in_window->area_.StopSelection();
            ExitFromMode();
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - mouse_.last_click_time)
                .count() <
            global_opts_->GetOpt<int64_t>(kOptCursorStartHoldingInterval)) {
            mouse_.state = MouseState::kLeftHolding;
        } else {
            mouse_.last_click_time = now;
            mouse_.state = MouseState::kLeftNotReleased;
        }
    } else if (mouse_.state == MouseState::kLeftNotReleased) {
        mouse_.state = MouseState::kLeftHolding;
        if (win) {
            if (win == prev_win && !(prev_pos == cursor_.pos)) {
                cursor_.in_window->area_.StartSelection(prev_pos);
                GotoMode(Mode::kVisual);
            }
        }
    } else if (mouse_.state == MouseState::kLeftHolding) {
        if (cursor_.in_window->area_.IsSelectionActive()) {
            if (win) {
                cursor_.in_window->area_.SelectionFollowCursor();
            }
            return;
        }

        if (win) {
            if (win == prev_win && !(prev_pos == cursor_.pos)) {
                cursor_.in_window->area_.StartSelection(prev_pos);
                GotoMode(Mode::kVisual);
            }
        }
    }
}

void Editor::HandleRelease(int s_row, int s_col) {
    (void)s_row, (void)s_col;
    mouse_.state = MouseState::kReleased;
}

void Editor::HandleMouse() {
    // Only non peel
    if (IsPeel(mode_)) {
        return;
    }

    Terminal::MouseInfo mouse_info = term_.EventMouseInfo();
    switch (mouse_info.t) {
        using mk = Terminal::MouseKey;
        case mk::kLeft: {
            HandleLeftClick(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kRight: {
            mouse_.state = MouseState::kRightNotReleased;
            cursor_.in_window->area_.StopSelection();
            break;
        }
        case mk::kMiddle: {
            mouse_.state = MouseState::kMiddleNotReleased;
            cursor_.in_window->area_.StopSelection();
            break;
        }
        case mk::kRelease: {
            HandleRelease(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kWheelDown: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                cursor_.in_window->ScrollRows(
                    global_opts_->GetOpt<int64_t>(kOptScrollRows));
            }
            // Not locate any area, do nothing
            break;
        }
        case mk::kWheelUp: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                cursor_.in_window->ScrollRows(
                    -global_opts_->GetOpt<int64_t>(kOptScrollRows));
            }
            // Not locate any area, do nothing
            break;
        }
    }
}

void Editor::HandleResize() { layout_manager_->ArrangeLayout(); }

void Editor::Draw() {
    // TODO: do not redraw not modified part
    // TODO: should we redraw if any events trigger?

    // First clear the screen so we don't need to print spaces for blank
    // screen parts
    term_.Clear();

    window_->Draw(highlight_search_);
    status_line_->Draw();
    peel_->Draw();

    // Put it at last so it can override some parts
    cmp_menu_->Draw();

    // Draw cursor
    if (cursor_.s_col == -1 && cursor_.s_row == -1) {
        // when not make visible
        term_.HideCursor();
    } else {
        term_.SetCursor(cursor_.s_col, cursor_.s_row);
    }

    term_.Present();
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    if (window_->area_.buffer_->state() == BufferState::kHaveNotRead) {
        try {
            window_->area_.buffer_->Load();
            // TODO: Not init if file is too big.
            syntax_parser_->SyntaxInit(window_->area_.buffer_);
        } catch (Exception& e) {
            MGO_LOG_ERROR("buffer {} : {}", window_->area_.buffer_->Name(),
                          e.what());
            // TODO: Maybe Notify the user
        }
    }

    layout_manager_->EnsureLayout();

    window_->area_.MakeSureViewValid();
    peel_->area_.MakeSureViewValid();
    if (!IsPeel(mode_)) {
        cursor_.in_window->MakeCursorVisible();
    } else {
        peel_->MakeCursorVisible();
    }
}

Window* Editor::LocateWindow(int s_col, int s_row) {
    if (window_->area_.In(s_col, s_row)) {
        return window_.get();
    }
    return nullptr;
}

Editor& Editor::GetInstance() {
    static Editor editor;
    return editor;
}

void Editor::Help(const std::string& doc_name) {
    auto p = Path(Path::GetAppRoot() + kDocsPath + doc_name);
    Buffer* b = buffer_manager_->FindBuffer(p);
    if (b == nullptr) {
        try {
            std::vector<std::string> all_docs =
                Path::ListUnderPath(Path::GetAppRoot() + kDocsPath);
            bool found = false;
            for (const auto& doc : all_docs) {
                if (doc == doc_name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                NotifyUser(fmt::format("Can't found doc: {}", doc_name));
                return;
            }
            b = buffer_manager_->AddBuffer(Buffer(global_opts_.get(), p, true));
        } catch (FSException& e) {
            MGO_LOG_ERROR("AllDocs error: {}", e.what());
            return;
        }
    }
    cursor_.in_window->AttachBuffer(b);
}

void Editor::Quit(bool force) {
    bool have_not_saved = false;
    for (auto buffer = buffer_manager_->Begin();
         buffer != buffer_manager_->End(); buffer = buffer->next_) {
        if (buffer->state() == BufferState::kModified) {
            have_not_saved = true;
            break;
        }
    }
    if (!have_not_saved || force) {
        loop_->EndLoop();
    } else {
        NotifyUser("Some buffers have not saved, force quit with 'q!'");
    }
}

void Editor::GotoPeel(Mode mode) {
    MGO_ASSERT(!IsPeel(mode_));

    if (mode == Mode::kPeelShow && peel_->area_.height_ == 1) {
        return;
    }

    cursor_.in_window->area_.b_view_->SaveCursorState(&cursor_);
    b_view_stored_for_edit = *(cursor_.in_window->area_.b_view_);
    cursor_.restore_from_peel = cursor_.in_window;
    cursor_.in_window = nullptr;
    GotoMode(mode);

    if (mode == Mode::kPeelCommand) {
        peel_->UserInputStart("");
    } else if (mode == Mode::kPeelSearch) {
        peel_->UserInputStart("Search: ");
    } else if (mode == Mode::kPeelShow) {
        cursor_.pos = Pos{0, 0};
    }
}

void Editor::ExitFromMode() {
    switch (mode_) {
        case Mode::kNormal:
            highlight_search_ = false;
            break;
        case Mode::kInsert: {
            cursor_.in_window->CursorGoLeft(Count());
            term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
            break;
        }
        case Mode::kVisual:
        case Mode::kVisualLine: {
            cursor_.in_window->area_.StopSelection();
            break;
        }
        case Mode::kOperatorPending: {
            break;
        }
        case Mode::kPeelCommand:
        case Mode::kPeelSearch:
        case Mode::kPeelShow: {
            MGO_ASSERT(cursor_.restore_from_peel);
            cursor_.in_window = cursor_.restore_from_peel;
            const TextArea& f = cursor_.in_window->area_;
            *(f.b_view_) = b_view_stored_for_edit;
            f.b_view_->RestoreCursorState(&cursor_, f.buffer_);
            highlight_search_ = false;
            break;
        }
        default:
            MGO_ASSERT("Can't reach here");
    }
    mode_ = Mode::kNormal;
}

void Editor::GotoMode(Mode mode) {
    if (mode_ != Mode::kNormal) {
        ExitFromMode();
    }
    if (mode == Mode::kInsert) {
        term_.SetCursorStyle(Terminal::CursorStyle::kLine);
    }
    mode_ = mode;
}

void Editor::SearchCurrentBuffer(const std::string& pattern) {
    // TODO: Maybe we can eliminate duplicate searching?
    if (cursor_.in_window) {
        cursor_.in_window->BuildSearchContext(pattern);
        CursorGoSearch(search_foward_, 1, true);
    } else {
        Window* w = cursor_.restore_from_peel;
        w->BuildSearchContext(pattern);
        // Just make buffer view move.
        // A little bit ugly, but just make sure we don't modify cursor, and
        // make the cursor state right accroding to the buffer state.
        Cursor c = cursor_;
        w->area_.b_view_->RestoreCursorState(&c, w->area_.buffer_);
        CursorState c_state(&c);
        bool has_result = w->area_.BufferViewGoSearchResult(
            w->b_search_context_, search_foward_, 1, true, c_state);
        c_state.SetCursor(&c);
        w->area_.b_view_->SaveCursorState(&c);
        highlight_search_ = has_result;
    }
}

void Editor::CursorGoSearch(bool next, size_t count, bool keep_current_if_one) {
    std::stringstream ss;
    Window* w = cursor_.in_window;
    auto& pattern = w->GetSearchPattern();
    BufferSearchState state =
        w->CursorGoSearchResult(next, count, keep_current_if_one);
    ss << "Searching \"" << pattern << "\" ";
    if (state.total == 0) {
        ss << "[No result]";
    } else {
        ss << "[" << state.i << "/" << state.total << "]";
        highlight_search_ = true;
    }
    NotifyUser(ss.str());
}

void Editor::TriggerCompletion(bool autocmp) {
    if (CompletionTriggered()) {
        CancellCompletion();
    }

    bool in_peel = IsPeel(mode_);
    if (!in_peel) {
        completer_ = window_->area_.buffer_->completer();
    } else {
        completer_ = &peel_->completer_;
    }

    if (completer_ == nullptr) {
        if (autocmp) {
            return;
        }
        if (!in_peel) {
            NotifyUser("No completion source");
        }
        return;
    }

    std::vector<std::string> entries;
    completer_->Suggest(cursor_.pos, entries);
    if (entries.empty()) {
        completer_->Cancel();
        if (!autocmp && !in_peel) {
            NotifyUser("No completion");
        }
        completer_ = nullptr;
        return;
    }

    cmp_menu_->SetEntries(std::move(entries));
    cmp_menu_->visible() = true;
    show_cmp_menu_ = true;
}

void Editor::CancellCompletion() {
    if (!CompletionTriggered()) {
        return;
    }

    completer_->Cancel();
    completer_ = nullptr;
    cmp_menu_->Clear();
    cmp_menu_->visible() = false;
}

bool Editor::CompletionTriggered() { return completer_ != nullptr; }

void*& Editor::ContextManager::GetContext(ContextID id) {
    return contexts_[id];
}
void Editor::ContextManager::FreeContext(ContextID id) { contexts_.erase(id); }

void Editor::PickBuffers() {
    GotoPeel();
    peel_->AddStringAtCursor("b ");
    TriggerCompletion(false);
}

void Editor::EditFile() {
    GotoPeel();
    peel_->AddStringAtCursor("e ");
    TriggerCompletion(false);
}

void Editor::CommandHitEnter() {
    if (CompletionTriggered()) {
        if (completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
            kRetriggerCmp) {
            completer_ = nullptr;
            TriggerCompletion(true);
        } else {
            completer_ = nullptr;
        }
        return;
    }

    CommandArgs args;
    Command* c;
    Result res = command_manager_.EvalCommand(peel_->GetUserInput(), args, c);
    if (res == kCommandEmpty) {
        ExitFromMode();
        return;
    }

    if (res != kOk) {
        NotifyUser("Wrong Command");
        ExitFromMode();
        return;
    }

    // We first exit from peel, so we can use cursor_->in_window to get the
    // current window.
    ExitFromMode();
    c->f(args);
}

void Editor::SearchHitEnter() {
    ExitFromMode();
    SearchCurrentBuffer(std::string(peel_->GetUserInput()));
}

void Editor::CursorUp(size_t count) {
    if (CompletionTriggered()) {
        cmp_menu_->SelectPrev(count);
        show_cmp_menu_ = true;
    } else if (!IsPeel(mode_)) {
        cursor_.in_window->CursorGoUp(count);
    }
}

void Editor::CursorDown(size_t count) {
    if (CompletionTriggered()) {
        cmp_menu_->SelectNext(count);
        show_cmp_menu_ = true;
    } else if (!IsPeel(mode_)) {
        cursor_.in_window->CursorGoDown(count);
    }
}

void Editor::RemoveCurrentBuffer() {
    buffer_manager_->RemoveBuffer(cursor_.in_window->area_.buffer_);
}

void Editor::SaveCurrentBuffer() {
    try {
        Result res = cursor_.in_window->area_.buffer_->Write();
        if (res == kOk) {
            NotifyUser(fmt::format(
                "\"{}\" saved",
                cursor_.in_window->area_.buffer_->path().FileName()));
        } else if (res == kBufferNoBackupFile) {
            NotifyUser("Buffer no backup file");
        } else if (res == kBufferCannotLoad) {
            NotifyUser("Buffer can't load");
        } else if (res == kBufferReadOnly) {
            NotifyUser("Buffer read only");
        } else {
            assert("Can't reach here");
        }
    } catch (IOException& e) {
        std::string err_str = fmt::format("Buffer can't save: {}", e.what());
        NotifyUser(err_str);
    }
}

void Editor::SaveCurrentBufferAs(const Path& path) {
    Buffer* cur_b = cursor_.in_window->area_.buffer_;
    // We don't allow saving to the path of another buffer in order to avoid
    // some chaos.
    if (buffer_manager_->FindBuffer(path)) {
        NotifyUser("Same path buffer exists.");
        return;
    }
    try {
        Result res = cur_b->SaveAs(path);
        if (res == kBufferCannotLoad) {
            NotifyUser("Buffer can't load");
        } else if (res == kBufferReadOnly) {
            NotifyUser("Buffer read only");
        }
    } catch (IOException& e) {
        std::string err_str = fmt::format("Buffer can't save: {}", e.what());
        MGO_LOG_ERROR("{}", err_str);
        NotifyUser(err_str);
    }
}

void Editor::NotifyUser(std::string_view str) { peel_->ShowContent(str); }

void Editor::StartAutoCompletionTimer() {
    // We start a timer, every terminal event will cancel it.
    // So if the timer is timeout, user input seems over, but we shouldn't rely
    // on that and think the current cursor pos is a real grapheme
    // end(user-percieved). Good time to trigger a autocmp.
    if (!autocmp_trigger_timer_) {
        autocmp_trigger_timer_ = std::make_unique<SingleTimer>(
            std::chrono::milliseconds(
                global_opts_->GetOpt<int64_t>(kOptInputIdleTimeout)),
            [this] { TriggerCompletion(true); });
    }
    loop_->timer_manager_.StartTimer(autocmp_trigger_timer_.get());
}

void Editor::StartSearchOnTypeTimer() {
    if (global_opts_->GetOpt<bool>(kOptHighlightOnSearch)) {
        if (!search_on_type_timer_) {
            search_on_type_timer_ = std::make_unique<SingleTimer>(
                std::chrono::milliseconds(
                    global_opts_->GetOpt<int64_t>(kOptInputIdleTimeout)),
                [this] { TrySearchOnType(); });
        }
        loop_->timer_manager_.StartTimer(search_on_type_timer_.get());
    }
}

void Editor::TrySearchOnType() {
    // Whether user is still searching?
    // If yes we search the pattern, otherwise we just ignore.
    if (mode_ != Mode::kPeelSearch) {
        return;
    }
    SearchCurrentBuffer(std::string(peel_->GetUserInput()));
}

}  // namespace mango
