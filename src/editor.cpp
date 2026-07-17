#include "editor.h"

#include "character.h"
#include "clipboard.h"
#include "constants.h"
#include "fs.h"
#include "inttypes.h"  // IWYU pragma: keep
#include "options.h"
#include "term.h"

// TODO: show sth. to users if modify the readonly buffer.
namespace charxed {

namespace {
constexpr const char* kStartup[] = {
    R"(  ______ _    _ __   __)",
    R"( / ____/| |  | |\ \ / /)",
    R"(| |     | |__| | \ V /      CHARXED)",
    R"(| |     |  __  |  | |)",
    R"(| |____ | |  | | / ^ \   BY SHIXIN CHAI)",
    R"( \_____||_|  |_|/_/ \_\)",
    "",
    "",
    R"(tips:   :help [doc]<ENTER>   open docs)",
    R"(        :edit <file><ENTER>  edit files)",
    R"(        :quit<ENTER>         quit)",
    R"(        :smile<ENTER>        :))",
    "",
    R"(      Press any key to continue...)",
};

constexpr size_t kStartupWidth = 39;
constexpr size_t kStartupHeight = std::size(kStartup);

constexpr int kScreenMinWidth = 10;
constexpr int kScreenMinHeight = 3;
}  // namespace

void Editor::Init(std::unique_ptr<GlobalOpts> global_opts,
                  std::unique_ptr<InitOpts> init_opts) {
    global_opts_ = std::move(global_opts);

    loop_ = std::make_unique<EventLoop>(global_opts_.get());

    term_.Init(global_opts_.get());

    buffer_manager_ = std::make_unique<BufferManager>(&editor_event_manager_);
    clipboard_ = ClipBoard::CreateClipBoard(true);

    // Component
    status_line_ = std::make_unique<StatusLine>(&cursor_, global_opts_.get(),
                                                &mode_, &context_);
    peel_ = std::make_unique<MangoPeel>(&cursor_, global_opts_.get(),
                                        clipboard_.get(), buffer_manager_.get(),
                                        &command_manager_);
    cmp_menu_ = std::make_unique<CmpMenu>(&cursor_, global_opts_.get());
    explorer_ = std::make_unique<Explorer>(global_opts_.get(), &cursor_,
                                           &context_, buffer_manager_.get());

    syntax_parser_ = std::make_unique<SyntaxParser>(global_opts_.get());
    buffer_monitor_ = std::make_unique<BufferFSMonitor>(
        buffer_manager_.get(), &cursor_, syntax_parser_.get(), &mode_,
        &context_);

    // Create all buffers
    for (const char* path : init_opts->begin_files) {
        auto b = buffer_manager_->AddBuffer(
            Buffer(global_opts_.get(), std::string(path)));
        try {
            buffer_monitor_->MonitorBuffer(b);
        } catch (OSException& e) {
            NotifyUser(fmt::format("Monitor buffer error: {}", e.what()));
        }
    }

    // Create the first window.
    // If no buffer then create one no file backup buffer.
    Buffer* buf;
    if (buffer_manager_->Begin() != nullptr) {
        buf = buffer_manager_->Begin();
    } else {
        buf = buffer_manager_->AddBuffer({global_opts_.get()});
    }
    // CHX_LOG_DEBUG("buffer {}", buf->Name());
    text_window_ = std::make_unique<TextWindow>(
        &cursor_, global_opts_.get(), syntax_parser_.get(), clipboard_.get(),
        buffer_manager_.get());

    // Set Cursor in the first window
    cursor_.t_win = text_window_.get();
    cursor_.focused = cursor_.t_win;
    text_window_->AttachBuffer(buf);

    // Layout
    layout_manager_ = std::make_unique<LayoutManager>(
        text_window_.get(), status_line_.get(), peel_.get(), explorer_.get(),
        &context_);
    layout_manager_->ArrangeLayout();
    explorer_->Init(layout_manager_.get());

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
            auto buffer = reinterpret_cast<Buffer*>(arg);
            if (!buffer->path().Empty()) {
                buffer_monitor_->UnmonitorBuffer(buffer);
            }
            text_window_->OnBufferDelete(buffer);
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
                                         if (!command_prompt_)
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
        if (!need_redraw_) {
            need_redraw_ = true;
            return;
        }

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
        CHX_ASSERT(e & kEventRead);
        if (e & (kEventClose | kEventError)) {
            throw TermException("{}", "Poll event close or event error");
        }

        // We use a while to eat all events.
        // Usually, there will be only one event a time.
        // Bracketed Paste and multi-codepoint grapheme will lead to multiple
        // events; Otherwise, multiple events usually means that we meet a slow
        // machine, bad network or bad routines executing in the main thread too
        // long, and we should handle it(A lot of event-based system
        // usually handle all events in one frame/iteration, e.g. SDL3 manual
        // recommand users to do so https://wiki.libsdl.org/SDL3/SDL_PollEvent).
        while (term_.Poll(0)) {
            show_cmp_menu_ = false;
            if (autocmp_trigger_timer_ &&
                autocmp_trigger_timer_->IsTimingOn()) {
                loop_->timer_manager_.StopTimer(autocmp_trigger_timer_.get());
                CHX_ASSERT(!autocmp_trigger_timer_->IsTimingOn());
            }

            multirow_peel_keep_ = false;

            // Handle it and do sth
            switch (term_.WhatEvent()) {
                case Terminal::EventType::kKey:
                    if (in_bracketed_paste) {
                        HandleBracketedPaste(bracketed_paste_buffer);
                    } else {
                        HandleKey();
                    }
                    break;
                case Terminal::EventType::kMouse:
                    HandleMouse();
                    break;
                case Terminal::EventType::kResize:
                    HandleResize();
                    break;
                case Terminal::EventType::kBracketedPasteOpen:
                    in_bracketed_paste = true;
                    break;
                case Terminal::EventType::kBracketedPasteClose:
                    in_bracketed_paste = false;
                    if (IsPeel(mode_)) {
                        peel_->AddStringAtCursor(
                            std::move(bracketed_paste_buffer));
                        layout_manager_->ArrangeLayout();
                    } else {
                        cursor_.t_win->AddStringAtCursor(
                            std::move(bracketed_paste_buffer));
                    }
                    bracketed_paste_buffer = "";
                    break;
            }
        }
        term_.PolledOutUnset();

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

    auto buffer_fs_event_handler = [this](Event e) {
        if (e & (kEventClose | kEventError)) {
            throw FSException("{}", "Poll event close or event error");
        }

        CHX_ASSERT(e & kEventRead);
        if (!buffer_monitor_->HandleFsEvents()) {
            need_redraw_ = false;
        }
    };

    EventInfo term_tty;
    EventInfo term_resize;
    EventInfo buffer_fs;
    term_.GetFDs(term_tty.fd, term_resize.fd);
    buffer_fs.fd = buffer_monitor_->fd();
    term_tty.Interesting_events |= kEventRead;
    term_resize.Interesting_events |= kEventRead;
    buffer_fs.Interesting_events |= kEventRead;
    term_tty.handler = term_handler;
    term_resize.handler = std::move(term_handler);
    buffer_fs.handler = std::move(buffer_fs_event_handler);

    loop_->BeforePoll(std::move(before_poll));
    loop_->AddEventHandler(std::move(term_tty));
    loop_->AddEventHandler(std::move(term_resize));
    loop_->AddEventHandler(std::move(buffer_fs));

    loop_->Loop();
}

void Editor::PrintKey(const Terminal::KeyInfo& key_info) {
    bool ctrl = key_info.mod & Terminal::kCtrl;
    bool shift = key_info.mod & Terminal::kShift;
    bool alt = key_info.mod & Terminal::kAlt;
    bool motion = key_info.mod & Terminal::kMotion;
    (void)ctrl, (void)shift, (void)alt, (void)motion;
    char c[kMaxBytesUtf8Codepoint + 1];
    int len = UnicodeToUtf8(key_info.codepoint, c);
    c[len] = '\0';
    CHX_LOG_DEBUG(
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
        CHX_LOG_INFO("Unknown Special key in bracketed paste");
    } else {
        char c[kMaxBytesUtf8Codepoint + 1];
        int len = UnicodeToUtf8(key_info.codepoint, c);
        c[len] = '\0';
        CHX_ASSERT(len > 0);
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

    // If the editor is in count state, means user have already input a seq of
    // numbers, we calc the input here.
    // FIXME: count can be calc in kKeyseqMatched
    if (count_ != 0 && !key_info.IsSpecialKey() && key_info.codepoint >= '0' &&
        key_info.codepoint <= '9') {
        count_ = count_ * 10 + key_info.codepoint - '0';
        return;
    }

    // If key info is a pure codepoint, we treat one codepoint as a key event,
    // not a grapheme cluster.
    // 1. It's a limitation on terminals now. We can't detect graphemes on
    // input event reliably, especially on ssh.
    // 2. Users can input single codepoints.
    // 3. Our keymaps usually use special keys or ascii characters, which
    // users will be aware of.
    Result res = kKeyseqError;
    Keyseq* handler;
    res = keymap_manager_.FeedKey(key_info, handler);
    if (res == kKeyseqDone) {
        Mode last_mode = mode_;
        handler->f();
        if (last_mode != Mode::kOperatorPending) {
            if (mode_ == Mode::kOperatorPending) {
                // key seq like 2d2l -> d4l
                op_pending_stored_count_ = count_;
            }
        } else {
            CHX_ASSERT(context_ == Context::kEditor);
            if (cursor_.t_win->IsSelectionActive()) {
                switch (pending_operator_) {
                    case Operator::kYank:
                        cursor_.t_win->Copy();
                        break;
                    case Operator::kDelete:
                        cursor_.t_win->Cut();
                        break;
                    case Operator::kIndent:
                        cursor_.t_win->IndentSelection(1);
                        break;
                    case Operator::kUnindent:
                        cursor_.t_win->UnindentSelection(1);
                        break;
                }
            }
            ExitFromMode();
        }
        count_ = 0;
    } else if (res == kKeyseqMatched) {
        // CHX_LOG_DEBUG("keymap matched");

        // Encounter a sequnce, we let multirow peel stay.
        if (!IsPeel(mode_) && peel_->area_.height_ > 1) {
            multirow_peel_keep_ = true;
        }
    } else if (res == kKeyseqError) {
        if (mode_ == Mode::kOperatorPending) {
            ExitFromMode();
            return;
        }

        if (key_info.IsSpecialKey()) {
            count_ = 0;
            return;
        }

        // Pure codepoints that are not handled by the keymap manager.
        if (InsertLike(mode_)) {
            // We try to collect all pure codepoints in one string and commit it
            // to the buffer.

            // Optimize for single codepoint input, most cases.
            size_t len;
            char c[kMaxBytesUtf8Codepoint];
            len = UnicodeToUtf8(key_info.codepoint, c);
            CHX_ASSERT(len > 0);

            std::string long_input;
            Result feed_key_res = kKeyseqError;
            while (term_.Poll(0)) {
                if (term_.WhatEvent() != Terminal::EventType::kKey) {
                    term_.PendCurrentEvent();
                    break;
                }
                key_info = term_.EventKeyInfo();
                if (key_info.IsSpecialKey()) {
                    term_.PendCurrentEvent();
                    break;
                }

                // Multiple codepoint arrive at a time, we should filter them
                // with keymaps, then treat them as pure input.
                feed_key_res = keymap_manager_.FeedKey(key_info, handler);
                if (feed_key_res == kKeyseqDone) {
                    break;
                } else if (feed_key_res == kKeyseqMatched) {
                    // Encounter a sequnce, we let multirow peel stay.
                    if (!IsPeel(mode_) && peel_->area_.height_ > 1) {
                        multirow_peel_keep_ = true;
                    }
                    break;
                } else if (feed_key_res == kKeyseqError) {
                    if (long_input.size() == 0) {
                        long_input.append(c, len);
                    }
                    int codepoint_len = UnicodeToUtf8(key_info.codepoint, c);
                    long_input.append(c, codepoint_len);
                    len += codepoint_len;
                } else {
                    CHX_ASSERT(false);
                }
            }

            const char* buf = long_input.empty() ? c : long_input.data();
            if (IsPeel(mode_)) {
                res = peel_->AddStringAtCursor(std::string_view(buf, len));
                layout_manager_->ArrangeLayout();
            } else {
                // We only support insert mode in kEditor context.
                CHX_ASSERT(context_ == Context::kEditor);
                res = cursor_.t_win->AddStringAtCursor(
                    std::string_view(buf, len));
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
                    CHX_ASSERT(false);
                    break;
            }
            editor_event_manager_.EmitEvent(ev, nullptr);

            if (feed_key_res == kKeyseqDone) {
                handler->f();
            }
        } else if (key_info.codepoint >= '1' && key_info.codepoint <= '9' &&
                   count_ == 0) {
            count_ = count_ * 10 + key_info.codepoint - '0';
        }
    } else {
        CHX_ASSERT(false);
    }
}

void Editor::HandleLeftClick(int s_row, int s_col) {
    Window* win = LocateWindow(s_col, s_row);
    if (!win) {
        return;
    }

    Window* prev_win = cursor_.focused;
    Pos prev_pos = cursor_.pos;  // TODO: check this logic.
    switch (context_) {
        case Context::kEditor:
            cursor_.t_win = static_cast<TextWindow*>(win);
        default:
            break;
    }
    win->SetCursorHint(s_row, s_col);

    // TODO: add a insert-select mode?
    if (mouse_.state == MouseState::kReleased) {
        if (win->IsSelectionActive()) {
            win->StopSelection();
            ExitFromMode();
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - mouse_.last_click_time)
                .count() <
            global_opts_->GetOpt<int64_t>(kOptCursorStartHoldingInterval)) {
            mouse_.state = MouseState::kLeftHolding;
            if (win->DoubleClick() == kSelectionStarted) {
                GotoMode(Mode::kSelect);
            }
        } else {
            mouse_.last_click_time = now;
            mouse_.state = MouseState::kLeftNotReleased;
        }
    } else if (mouse_.state == MouseState::kLeftNotReleased) {
        mouse_.state = MouseState::kLeftHolding;
        if (win == prev_win && !(prev_pos == cursor_.pos)) {
            if (!win->StartSelection(prev_pos)) return;
            GotoMode(Mode::kSelect);
        }
    } else if (mouse_.state == MouseState::kLeftHolding) {
        if (win->IsSelectionActive()) {
            win->SelectionFollowCursor();
            return;
        }

        if (win == prev_win && !(prev_pos == cursor_.pos)) {
            if (!win->StartSelection(prev_pos)) return;
            GotoMode(Mode::kSelect);
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
            cursor_.focused->StopSelection();
            break;
        }
        case mk::kMiddle: {
            mouse_.state = MouseState::kMiddleNotReleased;
            cursor_.focused->StopSelection();
            break;
        }
        case mk::kRelease: {
            HandleRelease(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kWheelDown: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                win->ScrollRows(global_opts_->GetOpt<int64_t>(kOptScrollRows));
            }
            // Not locate any area, do nothing
            break;
        }
        case mk::kWheelUp: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                win->ScrollRows(-global_opts_->GetOpt<int64_t>(kOptScrollRows));
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

    switch (context_) {
        case Context::kEditor:
            text_window_->Draw(highlight_search_);
            break;
        case Context::kExplorer:
            explorer_->Draw(highlight_search_);
            break;
        default:
            CHX_ASSERT(false);
            CHX_LOG_ERROR("Can't reach here");
    }
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
    if (text_window_->area_.buffer_->state() == BufferState::kHaveNotRead) {
        try {
            text_window_->area_.buffer_->Load();
            // TODO: Not init if file is too big.
            // Or just schedule the init.
            TSTree* ts_tree =
                syntax_parser_->SyntaxInit(text_window_->area_.buffer_);
            text_window_->area_.buffer_->ts_tree() = ts_tree;
        } catch (Exception& e) {
            NotifyUser(fmt::format("buffer {} load error: {}",
                                   text_window_->area_.buffer_->Name(),
                                   e.what()));
        }
    }

    peel_->area_.MakeSureViewValid();
    switch (context_) {
        case Context::kEditor:
            text_window_->area_.MakeSureViewValid();
            if (!IsPeel(mode_)) {
                cursor_.t_win->MakeCursorVisible();
            } else {
                peel_->MakeCursorVisible();
            }
            break;
        case Context::kExplorer:
            if (!IsPeel(mode_)) {
                explorer_->MakeCursorVisible();
            } else {
                peel_->MakeCursorVisible();
            }
            break;
        default:
            CHX_ASSERT(false);
            CHX_LOG_ERROR("Can't reach here");
    }
}

Window* Editor::LocateWindow(int s_col, int s_row) {
    switch (context_) {
        case Context::kEditor:
            if (text_window_->area_.In(s_col, s_row)) {
                return text_window_.get();
            }
            break;
        case Context::kExplorer:
            if (explorer_->In(s_col, s_row)) {
                return explorer_.get();
            }
            break;
        default:
            CHX_ASSERT(false);
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
                ListUnderDirectory(Path::GetAppRoot() + kDocsPath);
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
            CHX_LOG_ERROR("AllDocs error: {}", e.what());
            return;
        }
    }
    cursor_.t_win->AttachBuffer(b);
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
        Prompt("Some buffers have not saved, force quit[y/n]?",
               [this](std::string_view s) {
                   if (s == "y") {
                       loop_->EndLoop();
                   }
               });
    }
}

void Editor::GotoPeel(Mode mode) {
    CHX_ASSERT(!IsPeel(mode_));

    if (mode == Mode::kPeelShow && peel_->area_.height_ == 1) {
        return;
    }

    cursor_.focused->SaveView();
    GotoMode(mode);

    if (mode == Mode::kPeelCommand) {
        peel_->UserInputStart("");
    } else if (mode == Mode::kPeelSearch) {
        peel_->UserInputStart(search_foward_ ? "Forward: " : "Backward: ");
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
            term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
            break;
        }
        case Mode::kSelect:
            [[fallthrough]];
        case Mode::kSelectLine: {
            cursor_.focused->StopSelection();
            break;
        }
        case Mode::kOperatorPending: {
            break;
        }
        case Mode::kPeelCommand:
            [[fallthrough]];
        case Mode::kPeelSearch:
            term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
            peel_->SetHistoryCursorToEnd();
            cursor_.focused->StopSelection();
            [[fallthrough]];
        case Mode::kPeelShow: {
            cursor_.focused->RestoreView();
            highlight_search_ = false;
            break;
        }
        default:
            CHX_ASSERT("Can't reach here");
    }
    mode_ = Mode::kNormal;
}

void Editor::GotoMode(Mode mode) {
    switch (mode) {
        case Mode::kInsert:
            if (mode_ != Mode::kNormal) {
                ExitFromMode();
            }
            term_.SetCursorStyle(Terminal::CursorStyle::kLine);
            break;
        case Mode::kPeelCommand:
            [[fallthrough]];
        case Mode::kPeelSearch:
            if (mode_ == Mode::kSelect || mode_ == Mode::kSelectLine) {
                selection_range_for_seach_or_cmd_ =
                    cursor_.focused->SelectionRange();
            } else if (mode_ != Mode::kNormal) {
                ExitFromMode();
            }
            term_.SetCursorStyle(Terminal::CursorStyle::kLine);
            break;
        default:
            if (mode_ != Mode::kNormal) {
                ExitFromMode();
            }
            break;
    }
    mode_ = mode;
}

void Editor::SearchCurrentWindow(const std::string& pattern) {
    // TODO: Maybe we can eliminate duplicate searching?
    if (!IsPeel(mode_)) {
        cursor_.focused->BuildSearchContext(
            pattern, OptionalToPtr(selection_range_for_seach_or_cmd_));
        CursorGoSearch(search_foward_, 1, true);
    } else {
        auto w = cursor_.focused;
        w->BuildSearchContext(pattern,
                              OptionalToPtr(selection_range_for_seach_or_cmd_));
        highlight_search_ = w->ViewGoSearchResult(search_foward_, 1, true);
    }
}

void Editor::CursorGoSearch(bool next, size_t count, bool keep_current_if_one) {
    std::stringstream ss;
    auto w = cursor_.focused;
    auto& pattern = w->GetSearchPattern();
    SearchState state =
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
        completer_ = text_window_->area_.buffer_->completer();
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

void Editor::CommandHitEnter() {
    std::string_view input = peel_->GetUserInput();
    if (command_prompt_) {
        prompt_handler_(input);
        command_prompt_ = false;
        ExitFromMode();
        return;
    }

    CommandArgs args;
    Command* c;
    Result res = command_manager_.EvalCommand(input, args, c);
    if (res == kCommandEmpty) {
        ExitFromMode();
        return;
    }

    peel_->AppendHistoryItem(mode_ == Mode::kPeelCommand
                                 ? MangoPeel::HistoryType::kCmd
                                 : MangoPeel::HistoryType::kSearch);

    if (res != kOk) {
        NotifyUser("Wrong Command");
        ExitFromMode();
        return;
    }

    // We first exit from peel
    ExitFromMode();
    c->f(args);
    selection_range_for_seach_or_cmd_.reset();
}

void Editor::SearchHitEnter() {
    ExitFromMode();
    auto input = peel_->GetUserInput();
    if (!input.empty()) {
        peel_->AppendHistoryItem(MangoPeel::HistoryType::kSearch);
    }
    SearchCurrentWindow(std::string(input));
    selection_range_for_seach_or_cmd_.reset();
}

void Editor::RemoveCurrentBuffer() {
    buffer_manager_->RemoveBuffer(cursor_.t_win->area_.buffer_);
}

void Editor::SaveCurrentBuffer() {
    try {
        Result res = cursor_.t_win->area_.buffer_->Save();
        if (res == kOk) {
            ;
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
    Buffer* cur_b = cursor_.t_win->area_.buffer_;
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
        CHX_LOG_ERROR("{}", err_str);
        NotifyUser(err_str);
    }
}

void Editor::NotifyUser(std::string_view str) {
    peel_->ShowContent(str);
    layout_manager_->ArrangeLayout();
}

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
    SearchCurrentWindow(std::string(peel_->GetUserInput()));
}

Character Editor::CombineACharacterFromInput(Codepoint init_cp) {
    Character c;
    utf8proc_int32_t state = 0;
    c.Push(init_cp);
    while (term_.Poll(0)) {
        if (term_.WhatEvent() != Terminal::EventType::kKey) {
            term_.PendCurrentEvent();
            break;
        }
        auto key_info = term_.EventKeyInfo();
        if (key_info.IsSpecialKey()) {
            term_.PendCurrentEvent();
            break;
        }
        if (utf8proc_grapheme_break_stateful(
                c.Codepoints()[c.CodePointCount() - 1], key_info.codepoint,
                &state)) {
            term_.PendCurrentEvent();
            break;
        }
        c.Push(key_info.codepoint);
    }
    return c;
}

void Editor::Prompt(const std::string& prefix, const PromptHandler& handler) {
    GotoPeel(Mode::kPeelCommand);
    peel_->UserInputStart(prefix);
    command_prompt_ = true;
    prompt_handler_ = handler;
}

void Editor::StartupScreen() {
    if (buffer_manager_->Begin()->next_ != buffer_manager_->End() ||
        !buffer_manager_->Begin()->path().Empty()) {
        return;
    }

    size_t h = term_.Height();
    size_t w = term_.Width();

    if (h < kStartupHeight || w < kStartupWidth) {
        return;
    }

    size_t row0 = (h - kStartupHeight) / 2;
    size_t col0 = (w - kStartupWidth) / 2;

    term_.Clear();
    term_.HideCursor();
    auto theme = global_opts_->GetOpt<Theme>(kOptTheme);
    for (size_t r = row0; r < row0 + kStartupHeight; r++) {
        term_.Print(col0, r, theme[kNormal], kStartup[r - row0]);
    }
    term_.Present();

    term_.Poll(-1);
    // If this is a resize event, we should handle it.
    if (term_.WhatEvent() == Terminal::EventType::kResize) {
        HandleResize();
    }
}

void Editor::Edit(const std::string& path) {
    Path p = Path(path);
    Buffer* b = buffer_manager_->FindBuffer(p);
    if (b) {
        cursor_.t_win->AttachBuffer(b);
        return;
    }
    b = buffer_manager_->AddBuffer(Buffer(global_opts_.get(), std::move(p)));
    try {
        buffer_monitor_->MonitorBuffer(b);
    } catch (OSException& e) {
        NotifyUser(fmt::format("Monitor buffer error: {}", e.what()));
    }
    cursor_.t_win->AttachBuffer(b);
}

void Editor::OpenExplorer() {
    CHX_ASSERT(!IsPeel(mode_));
    if (context_ != Context::kExplorer) {
        cursor_.focused->SaveView();
        context_ = Context::kExplorer;
        explorer_->RestoreView();
        cursor_.focused = explorer_.get();
        layout_manager_->ArrangeLayout();
    }
}

void Editor::QuitExplorer() {
    CHX_ASSERT(!IsPeel(mode_));
    CHX_ASSERT(context_ == Context::kExplorer);
    explorer_->SaveView();
    cursor_.t_win->RestoreView();
    context_ = Context::kEditor;
    cursor_.focused = cursor_.t_win;
    layout_manager_->ArrangeLayout();
}

void Editor::EnsureInEditorContext() {
    switch (context_) {
        case Context::kEditor:
            break;
        case Context::kExplorer: {
            QuitExplorer();
            break;
        }
        default:
            CHX_ASSERT(false);
            break;
    }
}

}  // namespace charxed
