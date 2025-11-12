#include "editor.h"

#include <inttypes.h>

#include "character.h"
#include "clipboard.h"
#include "constants.h"
#include "fs.h"
#include "options.h"
#include "term.h"

namespace mango {

void Editor::Loop(std::unique_ptr<Options> options,
                  std::unique_ptr<InitOptions> init_options) {
    MGO_LOG_DEBUG("Loop init");
    options_ = std::move(options);
    clipboard_ = ClipBoard::CreateClipBoard(true);
    status_line_ =
        std::make_unique<StatusLine>(&cursor_, options_.get(), &mode_);
    peel_ =
        std::make_unique<MangoPeel>(&cursor_, options_.get(), clipboard_.get());
    cmp_menu_ = std::make_unique<CmpMenu>(&cursor_, options_.get());

    syntax_parser_ = std::make_unique<SyntaxParser>(options_.get());

    term_.Init(options_.get());

    // Create all buffers
    for (const char* path : init_options->begin_files) {
        buffer_manager_.AddBuffer(Buffer(options_.get(), path));
    }

    // Create the first window.
    // If no buffer then create one no file backup buffer.
    Buffer* buf;
    if (buffer_manager_.FirstBuffer() != nullptr) {
        buf = buffer_manager_.FirstBuffer();
    } else {
        buf = buffer_manager_.AddBuffer({options_.get()});
    }
    MGO_LOG_DEBUG("buffer %s", zstring_view_c_str(buf->path().ThisPath()));
    window_ = std::make_unique<Window>(buf, &cursor_, options_.get(),
                                       syntax_parser_.get(), clipboard_.get());

    // Set Cursor in the first window
    cursor_.in_window = window_.get();

    // manually trigger resizing to create the layout
    Resize(term_.Width(), term_.Height());

    InitKeymaps();
    InitCommands();

    // init options end of life
    init_options.reset();

    bool in_bracketed_paste = false;
    std::string bracketed_paste_buffer;

    // Event Loop
    // TODO: support custom cursor blinking
    while (!quit_) {
        if (have_event_) {
            PreProcess();
            Draw();
        }

        // Poll a new Event
        have_event_ = term_.Poll(options_->poll_event_timeout_ms);
        if (!have_event_) {
            continue;
        }

        // Handle it and do sth
        auto& e = term_.GetEvent();
        if (term_.EventIsKey(e)) {
            HandleKey(e,
                      in_bracketed_paste ? &bracketed_paste_buffer : nullptr);
        } else if (term_.EventIsMouse(e)) {
            HandleMouse(e);
        } else if (term_.EventIsResize(e)) {
            HandleResize(e);
        } else {
            Terminal::EventType t = term_.WhatEvent(e);
            if (t == Terminal::EventType::kBracketedPasteOpen) {
                in_bracketed_paste = true;
            } else if (t == Terminal::EventType::kBracketedPasteClose) {
                in_bracketed_paste = false;
                if (IsPeel(mode_)) {
                    peel_->AddStringAtCursor(std::move(bracketed_paste_buffer));
                } else {
                    cursor_.in_window->AddStringAtCursor(
                        std::move(bracketed_paste_buffer));
                }
                bracketed_paste_buffer = "";
            }
        }
    }
}

void Editor::InitKeymaps() {
    std::vector<Mode> efp = {Mode::kEdit, Mode::kFind, Mode::kPeelCommand};

    // quit
    keymap_manager_.AddKeyseq("<c-q>", {[this] { Quit(); }}, kAllModes);

    // peel
    keymap_manager_.AddKeyseq("<c-p>", {[this] { GotoPeel(); }}, {Mode::kEdit});
    keymap_manager_.AddKeyseq(
        "<esc>", {[this] { ExitFromMode(); }},
        {Mode::kPeelCommand, Mode::kFind, Mode::kPeelShow});
    keymap_manager_.AddKeyseq(
        "<bs>", {[this] { peel_->DeleteCharacterBeforeCursor(); }},
        {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<c-w>",
                              {[this] { peel_->DeleteWordBeforeCursor(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<tab>", {[this] { peel_->TabAtCursor(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<enter>", {[this] {
                                  CommandArgs args;
                                  Command* c;
                                  Result res = command_manager_.EvalCommand(
                                      peel_->GetContent(), args, c);
                                  if (res != kOk) {
                                      peel_->SetContent("Wrong Command");
                                      ExitFromMode();
                                      return;
                                  }
                                  ExitFromMode();
                                  c->f(args);
                              }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<enter>", {[this] {
                                  // TODO
                                  void(this);
                              }},
                              {Mode::kPeelShow});
    keymap_manager_.AddKeyseq("<left>", {[this] { peel_->CursorGoLeft(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<right>", {[this] { peel_->CursorGoRight(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<c-left>",
                              {[this] { peel_->CursorGoPrevWord(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<c-right>",
                              {[this] { peel_->CursorGoNextWordEnd(true); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<home>", {[this] { peel_->CursorGoHome(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<end>", {[this] { peel_->CursorGoEnd(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<c-c>", {[this] { peel_->Copy(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeyseq("<c-v>", {[this] { peel_->Paste(); }},
                              {Mode::kPeelCommand});

    // Buffer manangement
    keymap_manager_.AddKeyseq("<c-b><c-n>",
                              {[this] { cursor_.in_window->NextBuffer(); }});
    keymap_manager_.AddKeyseq("<c-b><c-p>",
                              {[this] { cursor_.in_window->PrevBuffer(); }});
    keymap_manager_.AddKeyseq(
        "<c-b><c-d>", {[this] {
            Buffer* cur_buffer = cursor_.in_window->frame_.buffer_;
            if (cur_buffer->IsFirstBuffer() && cur_buffer->IsLastBuffer()) {
                cursor_.in_window->AttachBuffer(
                    buffer_manager_.AddBuffer({options_.get()}));
            } else if (cur_buffer->IsFirstBuffer()) {
                cursor_.in_window->NextBuffer();
            } else {
                cursor_.in_window->PrevBuffer();
            }
            syntax_parser_->OnBufferDelete(cur_buffer);
            buffer_manager_.RemoveBuffer(cur_buffer);
        }});

    keymap_manager_.AddKeyseq("<c-pgdn>",
                              {[this] { cursor_.in_window->NextBuffer(); }});
    keymap_manager_.AddKeyseq("<c-pgup>",
                              {[this] { cursor_.in_window->PrevBuffer(); }});

    // search
    keymap_manager_.AddKeyseq("<c-f>", {
                                           [this] {
                                               GotoPeel();
                                               peel_->AddStringAtCursor("s ");
                                           },
                                       });
    keymap_manager_.AddKeyseq("<c-p>", {[this] { SearchPrev(); }},
                              {Mode::kFind});
    keymap_manager_.AddKeyseq("<c-n>", {[this] { SearchNext(); }},
                              {Mode::kFind});

    // cmp
    keymap_manager_.AddKeyseq(
        "<c-k><c-c>", {[this] {
            // Trigger completion, A simple demo
            // TODO: support better
            TriggerCmp(
                [this](size_t size) {
                    std::stringstream ss;
                    ss << "User select " << size;
                    peel_->SetContent(ss.str());
                },
                [this] { peel_->SetContent("User cancel cmp"); },
                {"hello", "yes", "good"});
        }},
        {Mode::kEdit});
    keymap_manager_.AddKeyseq("<c-p>", {[this] { cmp_menu_->SelectPrev(); }},
                              {Mode::kCmp});
    keymap_manager_.AddKeyseq("<c-n>", {[this] { cmp_menu_->SelectNext(); }},
                              {Mode::kCmp});
    keymap_manager_.AddKeyseq("<up>", {[this] { cmp_menu_->SelectPrev(); }},
                              {Mode::kCmp});
    keymap_manager_.AddKeyseq("<down>", {[this] { cmp_menu_->SelectNext(); }},
                              {Mode::kCmp});
    keymap_manager_.AddKeyseq("<esc>", {[this] {
                                  cmp_menu_->Clear();
                                  cmp_cancel_callback_();
                                  ExitFromMode();
                              }},
                              {Mode::kCmp});
    keymap_manager_.AddKeyseq("<enter>", {[this] {
                                  cmp_accept_callback_(cmp_menu_->Accept());
                                  ExitFromMode();
                              }},
                              {Mode::kCmp});

    // edit
    keymap_manager_.AddKeyseq(
        "<bs>", {[this] { cursor_.in_window->DeleteAtCursor(); }});
    keymap_manager_.AddKeyseq(
        "<c-w>", {[this] { cursor_.in_window->DeleteWordBeforeCursor(); }});
    keymap_manager_.AddKeyseq("<tab>",
                              {[this] { cursor_.in_window->TabAtCursor(); }});
    keymap_manager_.AddKeyseq(
        "<enter>", {[this] { cursor_.in_window->AddStringAtCursor("\n"); }});
    keymap_manager_.AddKeyseq(
        "<c-s>", {[this] {
            try {
                Result res = cursor_.in_window->frame_.buffer_->Write();
                if (res == kBufferNoBackupFile) {
                    // TODO: notify user
                } else if (res == kBufferCannotLoad) {
                    // TODO: notify user
                }
            } catch (IOException& e) {
                MGO_LOG_ERROR("%s", e.what());
                // TODO: notify user
            }
        }});
    keymap_manager_.AddKeyseq("<c-z>", {[this] { cursor_.in_window->Undo(); }});
    keymap_manager_.AddKeyseq("<c-y>", {[this] { cursor_.in_window->Redo(); }});
    keymap_manager_.AddKeyseq("<c-c>", {[this] { cursor_.in_window->Copy(); }});
    keymap_manager_.AddKeyseq("<c-v>",
                              {[this] { cursor_.in_window->Paste(); }});
    keymap_manager_.AddKeyseq("<c-x>", {[this] { cursor_.in_window->Cut(); }});
    keymap_manager_.AddKeyseq("<c-a>",
                              {[this] { cursor_.in_window->SelectAll(); }});
    keymap_manager_.AddKeyseq("<c-k><c-l>",
                              {[this] { cursor_.in_window->SelectAll(); }});

    // naviagtion
    keymap_manager_.AddKeyseq("<left>",
                              {[this] { cursor_.in_window->CursorGoLeft(); }});
    keymap_manager_.AddKeyseq("<right>",
                              {[this] { cursor_.in_window->CursorGoRight(); }});
    keymap_manager_.AddKeyseq(
        "<c-left>", {[this] { cursor_.in_window->CursorGoPrevWord(); }});
    keymap_manager_.AddKeyseq("<c-right>", {[this] {
                                  cursor_.in_window->CursorGoNextWordEnd(true);
                              }});
    keymap_manager_.AddKeyseq("<up>",
                              {[this] { cursor_.in_window->CursorGoUp(); }});
    keymap_manager_.AddKeyseq("<down>",
                              {[this] { cursor_.in_window->CursorGoDown(); }});
    keymap_manager_.AddKeyseq(
        "<c-p>", {[this] { cursor_.in_window->CursorGoUp(); }}, {Mode::kEdit});
    keymap_manager_.AddKeyseq("<c-n>",
                              {[this] { cursor_.in_window->CursorGoDown(); }},
                              {Mode::kEdit});
    keymap_manager_.AddKeyseq("<home>",
                              {[this] { cursor_.in_window->CursorGoHome(); }});
    keymap_manager_.AddKeyseq("<end>",
                              {[this] { cursor_.in_window->CursorGoEnd(); }});
    keymap_manager_.AddKeyseq("<pgdn>", {[this] {
                                  cursor_.in_window->ScrollRows(
                                      cursor_.in_window->frame_.height_ - 1);
                              }});
    keymap_manager_.AddKeyseq("<pgup>", {[this] {
                                  cursor_.in_window->ScrollRows(
                                      -cursor_.in_window->frame_.height_ - 1);
                              }});
}

void Editor::InitCommands() {
    command_manager_.AddCommand({"h",
                                 "",
                                 {},
                                 [this](CommandArgs args) {
                                     (void)args;
                                     Help();
                                 },
                                 0});
    command_manager_.AddCommand(
        {"e",
         "",
         {Type::kString},
         [this](CommandArgs args) {
             const std::string& path_str = std::get<std::string>(args[0]);
             Path path(path_str);
             Buffer* b = buffer_manager_.FindBuffer(path);
             if (b) {
                 cursor_.in_window->AttachBuffer(b);
                 return;
             }
             cursor_.in_window->AttachBuffer(
                 buffer_manager_.AddBuffer(Buffer(options_.get(), path)));
         },
         1});
    command_manager_.AddCommand(
        {"s",
         "",
         {Type::kString},
         [this](CommandArgs args) {
             mode_ = Mode::kFind;
             MGO_LOG_DEBUG("search %s", std::get<std::string>(args[0]).c_str());
             cursor_.in_window->BuildSearchContext(
                 std::get<std::string>(args[0]));
             SearchNext();
         },
         1});
}

void Editor::HandleKey(const Terminal::Event& e,
                       std::string* bracketed_paste_buffer) {
    Terminal::KeyInfo key_info = term_.EventKeyInfo(e);

#ifndef NDEBUG
    bool ctrl = key_info.mod & Terminal::Mod::kCtrl;
    bool shift = key_info.mod & Terminal::Mod::kShift;
    bool alt = key_info.mod & Terminal::Mod::kAlt;
    bool motion = key_info.mod & Terminal::Mod::kMotion;
    char c[7];
    int len = UnicodeToUtf8(key_info.codepoint, c);
    c[len] = '\0';
    MGO_LOG_DEBUG(
        "ctrl %d shift %d alt %d motion %d special key %d codepoint "
        "\\U%08" PRIx32 " char %s",
        ctrl, shift, alt, motion, static_cast<int>(key_info.special_key),
        key_info.codepoint, c);
#endif  // !NDEBUG

    if (bracketed_paste_buffer) {
        if (key_info.IsSpecialKey() && key_info.mod == Terminal::kCtrl &&
            key_info.special_key == Terminal::SpecialKey::kEnter) {
            if (!IsPeel(mode_)) {
                bracketed_paste_buffer->push_back('\n');
            }
        } else if (key_info.IsSpecialKey()) {
            bracketed_paste_buffer->append(kReplacement);
            MGO_LOG_INFO("Unknown Special key in bracketed paste");
        } else {
            char c[7];
            int len = UnicodeToUtf8(key_info.codepoint, c);
            MGO_ASSERT(len > 0);
            bracketed_paste_buffer->append(c);
        }
        return;
    }

    Keyseq* handler;
    Result res = keymap_manager_.FeedKey(key_info, handler);
    if (res == kKeyseqError) {
        // pure characters
        // not handled by the keymap manager
        if (!key_info.IsSpecialKey() && key_info.mod == 0) {
            uint32_t codepoint = key_info.codepoint;

            char c[7];
            int len = UnicodeToUtf8(codepoint, c);
            MGO_ASSERT(len > 0);
            if (IsPeel(mode_)) {
                peel_->AddStringAtCursor(c);
            } else {
                cursor_.in_window->AddStringAtCursor(c);
            }
        }
        return;
    } else if (res == kKeyseqMatched) {
        MGO_LOG_DEBUG("keymap matched");
        return;
    }

    // Match Done
    handler->f();
}

void Editor::HandleLeftClick(int s_row, int s_col) {
    MGO_LOG_DEBUG("left mouse row %d, col %d", s_row, s_col);
    Window* win = LocateWindow(s_col, s_row);
    Window* prev_win = cursor_.in_window;
    Pos prev_pos = {cursor_.line, cursor_.byte_offset};
    if (win) {
        cursor_.in_window = win;
        win->SetCursorHint(s_row, s_col);
    }

    if (cursor_.state_ == CursorState::kReleased) {
        if (!win) {
            return;
        }
        if (cursor_.in_window->frame_.selection_.active) {
            cursor_.in_window->frame_.selection_.active = false;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - cursor_.last_click_time_)
                .count() < options_->cursor_start_holding_interval_ms) {
            cursor_.state_ = CursorState::kLeftHolding;
        } else {
            cursor_.last_click_time_ = now;
            cursor_.state_ = CursorState::kLeftNotReleased;
        }
    } else if (cursor_.state_ == CursorState::kLeftNotReleased) {
        cursor_.state_ = CursorState::kLeftHolding;
        if (win) {
            if (win == prev_win &&
                !(prev_pos == Pos{cursor_.line, cursor_.byte_offset})) {
                cursor_.in_window->frame_.selection_.active = true;
                cursor_.in_window->frame_.selection_.anchor = prev_pos;
                cursor_.in_window->frame_.selection_.head = {
                    cursor_.line, cursor_.byte_offset};
            }
        }
    } else if (cursor_.state_ == CursorState::kLeftHolding) {
        if (cursor_.in_window->frame_.selection_.active) {
            if (win) {
                cursor_.in_window->frame_.selection_.head = {
                    cursor_.line, cursor_.byte_offset};
            }
            return;
        }

        if (win) {
            if (win == prev_win &&
                !(prev_pos == Pos{cursor_.line, cursor_.byte_offset})) {
                cursor_.in_window->frame_.selection_.active = true;
                cursor_.in_window->frame_.selection_.anchor = prev_pos;
                cursor_.in_window->frame_.selection_.head = {
                    cursor_.line, cursor_.byte_offset};
            }
        }
    }
}

void Editor::HandleRelease(int s_row, int s_col) {
    MGO_LOG_DEBUG("release mouse row %d, col %d", s_row, s_col);

    cursor_.state_ = CursorState::kReleased;
}

void Editor::HandleMouse(const Terminal::Event& e) {
    // Only non peel
    if (IsPeel(mode_)) {
        return;
    }

    Terminal::MouseInfo mouse_info = term_.EventMouseInfo(e);
    switch (mouse_info.t) {
        using mk = Terminal::MouseKey;
        case mk::kLeft: {
            HandleLeftClick(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kRight: {
            cursor_.state_ = CursorState::kRightNotReleased;
            cursor_.in_window->frame_.selection_.active = false;
            break;
        }
        case mk::kMiddle: {
            cursor_.state_ = CursorState::kMiddleNotReleased;
            cursor_.in_window->frame_.selection_.active = false;
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
                    options_->scroll_rows_per_mouse_wheel_scroll);
            }
            // Not locate any area, do nothing
            break;
        }
        case mk::kWheelUp: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                cursor_.in_window->ScrollRows(
                    -options_->scroll_rows_per_mouse_wheel_scroll);
            }
            // Not locate any area, do nothing
            break;
        }
    }
}

// TODO: support multi window
void Editor::Resize(int width, int height) {
    Window* first_win = window_.get();
    first_win->frame_.col_ = 0;
    first_win->frame_.row_ = 0;
    first_win->frame_.width_ = width;
    first_win->frame_.height_ = height - 2;

    status_line_->row_ = height - 2;
    status_line_->width_ = width;

    peel_->frame_.row_ = height - 1;
    peel_->frame_.width_ = width;
    peel_->frame_.height_ = 1;
}

void Editor::HandleResize(const Terminal::Event& e) {
    Terminal::ResizeInfo resize_info = term_.EventResizeInfo(e);
    Resize(resize_info.width, resize_info.height);
}

void Editor::Draw() {
    // First clear the screen so we don't need to print spaces for blank
    // screen parts
    // TODO: do not redraw not modified part
    term_.Clear();
    Result ret = term_.SetCursor(cursor_.s_col, cursor_.s_row);
    if (ret == kTermOutOfBounds) {
        return;
    }

    if (window_->frame_.buffer_->IsLoad()) {
        window_->Draw();
    }

    status_line_->Draw();

    peel_->frame_.Draw();

    // Put it at last so it can override some parts
    cmp_menu_->Draw();

    term_.Present();
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    if (window_->frame_.buffer_->state() == BufferState::kHaveNotRead) {
        try {
            window_->frame_.buffer_->Load();
            syntax_parser_->SyntaxInit(window_->frame_.buffer_);
        } catch (FileCreateException& e) {
            window_->frame_.buffer_->state() = BufferState::kCannotCreate;
            MGO_LOG_ERROR("%s", e.what());
            // TODO: Notify the user
        } catch (IOException& e) {
            window_->frame_.buffer_->state() = BufferState::kCannotRead;
            MGO_LOG_ERROR(
                "buffer %s : %s",
                window_->frame_.buffer_->path().AbsolutePath().c_str(),
                e.what());
            // TODO: Notify the user
        }
    }

    if (!IsPeel(mode_)) {
        cursor_.in_window->MakeCursorVisible();
    } else {
        peel_->MakeCursorVisible();
    }
}

Window* Editor::LocateWindow(int s_col, int s_row) {
    if (window_->frame_.In(s_col, s_row)) {
        return window_.get();
    }
    return nullptr;
}

Editor& Editor::GetInstance() {
    static Editor editor;
    return editor;
}

void Editor::Help() {
    auto p = Path(Path::GetAppRoot() + kHelpPath);
    Buffer* b = buffer_manager_.FindBuffer(p);
    if (b) {
        cursor_.in_window->AttachBuffer(b);
        return;
    }
    b = buffer_manager_.AddBuffer(Buffer(options_.get(), p, true));
    cursor_.in_window->AttachBuffer(b);
}

void Editor::Quit() { quit_ = true; }

void Editor::GotoPeel() {
    MGO_ASSERT(!IsPeel(mode_));

    peel_->SetContent("");
    cursor_.in_window->frame_.buffer_->SaveCursorState(cursor_);
    cursor_.restore_from_peel = cursor_.in_window;
    cursor_.in_window = nullptr;
    cursor_.line = 0;
    cursor_.byte_offset = 0;
    mode_ = Mode::kPeelCommand;
}

void Editor::ExitFromMode() {
    if (IsPeel(mode_)) {
        MGO_ASSERT(cursor_.restore_from_peel);
        cursor_.in_window = cursor_.restore_from_peel;
        cursor_.in_window->frame_.buffer_->RestoreCursorState(cursor_);
    } else if (mode_ == Mode::kFind) {
        peel_->SetContent("");
        MGO_ASSERT(cursor_.in_window);
        cursor_.in_window->DestorySearchContext();
    } else if (mode_ == Mode::kCmp) {
        mode_ = mode_trigger_cmp_;
        return;
    }
    mode_ = Mode::kEdit;
}

void Editor::SearchNext() {
    MGO_ASSERT(mode_ == Mode::kFind);
    peel_->SetContent("");
    std::stringstream ss;
    auto& pattern = cursor_.in_window->GetSearchPattern();
    if (!pattern.empty()) {
        ss << "searching " << pattern << " ";
        Window::SearchState state =
            cursor_.in_window->CursorGoNextSearchResult();
        if (state.total == 0) {
            ss << "[No result]";
        } else {
            ss << "[" << state.i << "/" << state.total << "]";
        }
    } else {
        ss << "No searching pattern";
    }
    peel_->SetContent(ss.str());
}

void Editor::SearchPrev() {
    MGO_ASSERT(mode_ == Mode::kFind);
    peel_->SetContent("");
    std::stringstream ss;
    auto& pattern = cursor_.in_window->GetSearchPattern();
    if (!pattern.empty()) {
        ss << "searching " << pattern << " ";
        Window::SearchState state =
            cursor_.in_window->CursorGoPrevSearchResult();
        if (state.total == 0) {
            ss << "[No result]";
        } else {
            ss << "[" << state.i << "/" << state.total << "]";
        }
    } else {
        ss << "No searching pattern";
    }
    peel_->SetContent(ss.str());
}

void Editor::TriggerCmp(std::function<void(size_t)> accept_call_back,
                        std::function<void(void)> cancel_call_back,
                        std::vector<std::string> entries) {
    cmp_accept_callback_ = std::move(accept_call_back);
    cmp_cancel_callback_ = std::move(cancel_call_back);
    mode_trigger_cmp_ = mode_;
    cmp_menu_->SetEntries(std::move(entries));
    cmp_menu_->visible() = true;
    mode_ = Mode::kCmp;
}

void*& Editor::ContextManager::GetContext(ContextID id) {
    return contexts_[id];
}
void Editor::ContextManager::FreeContext(ContextID id) { contexts_.erase(id); }

}  // namespace mango