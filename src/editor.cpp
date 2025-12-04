#include "editor.h"

#include <inttypes.h>

#include "character.h"
#include "clipboard.h"
#include "constants.h"
#include "fs.h"
#include "options.h"
#include "term.h"

namespace mango {

using namespace std::chrono_literals;

static constexpr int kScreenMinWidth = 10;
static constexpr int kScreenMinHeight = 3;

void Editor::Init(std::unique_ptr<GlobalOpts> global_opts,
                  std::unique_ptr<InitOpts> init_opts) {
    global_opts_ = std::move(global_opts);

    loop_ = std::make_unique<EventLoop>(global_opts_.get());

    term_.Init(global_opts_.get());

    clipboard_ = ClipBoard::CreateClipBoard(true);
    status_line_ =
        std::make_unique<StatusLine>(&cursor_, global_opts_.get(), &mode_);
    peel_ = std::make_unique<MangoPeel>(&cursor_, global_opts_.get(),
                                        clipboard_.get(), &buffer_manager_);
    cmp_menu_ = std::make_unique<CmpMenu>(&cursor_, global_opts_.get());

    syntax_parser_ = std::make_unique<SyntaxParser>(global_opts_.get());

    // Create all buffers
    for (const char* path : init_opts->begin_files) {
        buffer_manager_.AddBuffer(
            Buffer(global_opts_.get(), std::string(path)));
    }

    // Create the first window.
    // If no buffer then create one no file backup buffer.
    Buffer* buf;
    if (buffer_manager_.Begin() != nullptr) {
        buf = buffer_manager_.Begin();
    } else {
        buf = buffer_manager_.AddBuffer({global_opts_.get()});
    }
    MGO_LOG_DEBUG("buffer %s", zstring_view_c_str(buf->Name()));
    window_ = std::make_unique<Window>(&cursor_, global_opts_.get(),
                                       syntax_parser_.get(), clipboard_.get(),
                                       &buffer_manager_);

    // Set Cursor in the first window
    cursor_.in_window = window_.get();
    window_->AttachBuffer(buf);

    // manually trigger resizing to create the layout
    Resize(term_.Width(), term_.Height());

    if (!global_opts_->GetOpt<bool>(kOptVi)) {
        mode_ = Mode::kEdit;
        InitKeymaps();
        InitCommands();
    } else {
        mode_ = Mode::kViNormal;
        InitKeymapsVi();
        InitCommandsVi();
    }

    if (!global_opts_->IsUserConfigValid()) {
        peel_->SetContent(
            "User config file error! Please check your configuration." +
            global_opts_->GetUserConfigErrorReportStrAndReleaseIt());
    }

    if (global_opts_->GetOpt<bool>(kOptCursorBlinking)) {
        cursor_.blinking_timer_ = std::make_unique<LoopTimer>(
            std::vector<std::chrono::milliseconds>{
                std::chrono::milliseconds(global_opts_->GetOpt<int64_t>(
                    kOptCursorBlinkingShowInterval)),
                std::chrono::milliseconds(global_opts_->GetOpt<int64_t>(
                    kOptCursorBlinkingHideInterval))},
            [this] { cursor_.show = !cursor_.show; });
    }
    autocmp_trigger_timer_ = std::make_unique<SingleTimer>(
        std::chrono::milliseconds(
            global_opts_->GetOpt<int64_t>(kOptAutoCmpTimeout)),
        [this] { TriggerCompletion(true); });

    // Register buffer remove callback
    buffer_manager_.AddOnBufferRemoveHandler([this](const Buffer* buffer) {
        window_->OnBufferDelete(buffer);
        syntax_parser_->OnBufferDelete(buffer);
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
            throw TermException("%s", "Poll event close or event error");
        }

        // We use a while to eat all events.
        // Usually, there will be only one event.
        // Bracketed Paste and multi-codepoint grapheme will lead to mult events;
        // Otherwise, multi events means we meet a slow machine, bad network or
        // a bad routine executes too long. I think we should handle it.
        while (term_.Poll(0)) {
            show_cmp_menu_ = false;
            if (autocmp_trigger_timer_->IsTimingOn()) {
                loop_->timer_manager_.StopTimer(autocmp_trigger_timer_.get());
            }

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

            // If autocmp trigger timer has started,
            // don't cancel it to avoid flash of cmp menu.
            if (!show_cmp_menu_ && !autocmp_trigger_timer_->IsTimingOn()) {
                CancellCompletion();
            }
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

    if (cursor_.blinking_timer_) {
        loop_->timer_manager_.StartTimer(cursor_.blinking_timer_.get());
    }

    loop_->Loop();
}

#define MGO_KEYMAP keymap_manager_.AddKeyseq

void Editor::InitKeymaps() {
    // quit
    MGO_KEYMAP("<c-q>", {[this] { Quit(); }}, kAllModes);

    // peel
    MGO_KEYMAP("<c-e>", {[this] { GotoPeel(); }}, {Mode::kEdit});
    MGO_KEYMAP("<esc>", {[this] {
                   if (!CompletionTriggered()) {
                       ExitFromMode();
                   }
               }},
               {Mode::kPeelCommand, Mode::kPeelShow});
    MGO_KEYMAP("<tab>", {[this] { TriggerCompletion(false); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<bs>", {[this] {
                   if (peel_->DeleteCharacterBeforeCursor() == kOk) {
                       StartAutoCompletionTimer();
                   }
               }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<c-w>", {[this] { peel_->DeleteWordBeforeCursor(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<enter>", {[this] { PeelHitEnter(); }}, {Mode::kPeelCommand});
    MGO_KEYMAP("<enter>", {[this] {
                   // TODO
                   void(this);
               }},
               {Mode::kPeelShow});
    MGO_KEYMAP("<left>", {[this] { peel_->CursorGoLeft(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<right>", {[this] { peel_->CursorGoRight(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<c-left>", {[this] { peel_->CursorGoWordBegin(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<c-right>", {[this] { peel_->CursorGoNextWordEnd(true); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<home>", {[this] { peel_->CursorGoHome(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<end>", {[this] { peel_->CursorGoEnd(); }},
               {Mode::kPeelCommand});
    MGO_KEYMAP("<c-c>", {[this] { peel_->Copy(); }}, {Mode::kPeelCommand});
    MGO_KEYMAP("<c-v>", {[this] { peel_->Paste(); }}, {Mode::kPeelCommand});

    // Buffer manangement
    MGO_KEYMAP("<c-b><c-b>", {[this] { PickBuffers(); }});
    MGO_KEYMAP("<c-b><c-e>", {[this] { EditFile(); }});
    MGO_KEYMAP("<c-b><c-n>", {[this] { cursor_.in_window->NextBuffer(); }});
    MGO_KEYMAP("<c-b><c-p>", {[this] { cursor_.in_window->PrevBuffer(); }});
    MGO_KEYMAP("<c-b><c-d>", {[this] { RemoveCurrentBuffer(); }});
    MGO_KEYMAP("<c-pgdn>", {[this] { cursor_.in_window->NextBuffer(); }});
    MGO_KEYMAP("<c-pgup>", {[this] { cursor_.in_window->PrevBuffer(); }});

    // search
    MGO_KEYMAP("<c-f>", {
                            [this] {
                                GotoPeel();
                                peel_->AddStringAtCursor("s ");
                            },
                        });
    MGO_KEYMAP("<c-k><c-p>", {[this] { SearchPrev(); }});
    MGO_KEYMAP("<c-k><c-n>", {[this] { SearchNext(); }});

    // cmp
    MGO_KEYMAP("<c-k><c-c>", {[this] {
                   if (!IsPeel(mode_) &&
                       (!cursor_.in_window->frame_.buffer_->IsLoad() ||
                        cursor_.in_window->frame_.buffer_->read_only())) {
                       return;
                   }
                   TriggerCompletion(false);
               }},
               kAllModes);

    // edit
    MGO_KEYMAP("<bs>", {[this] {
                   if (cursor_.in_window->DeleteAtCursor() == kOk &&
                       !cursor_.in_window->frame_.selection_.active) {
                       StartAutoCompletionTimer();
                   }
               }});
    MGO_KEYMAP("<c-w>",
               {[this] { cursor_.in_window->DeleteWordBeforeCursor(); }});
    MGO_KEYMAP("<tab>", {[this] { cursor_.in_window->TabAtCursor(); }});
    MGO_KEYMAP("<enter>", {[this] {
                   if (CompletionTriggered()) {
                       if (tmp_completer_->Accept(cmp_menu_->Accept(),
                                                  &cursor_) == kRetriggerCmp) {
                           tmp_completer_ = nullptr;
                           TriggerCompletion(true);
                       } else {
                           tmp_completer_ = nullptr;
                       }
                   } else {
                       cursor_.in_window->AddStringAtCursor("\n");
                   }
               }});
    MGO_KEYMAP("<c-s>", {[this] { SaveCurrentBuffer(); }});
    MGO_KEYMAP("<c-z>", {[this] { cursor_.in_window->Undo(); }});
    MGO_KEYMAP("<c-y>", {[this] { cursor_.in_window->Redo(); }});
    MGO_KEYMAP("<c-c>", {[this] { cursor_.in_window->Copy(); }});
    MGO_KEYMAP("<c-v>", {[this] { cursor_.in_window->Paste(); }});
    MGO_KEYMAP("<c-x>", {[this] { cursor_.in_window->Cut(); }});
    MGO_KEYMAP("<c-a>", {[this] { cursor_.in_window->SelectAll(); }});
    MGO_KEYMAP("<c-k><c-l>", {[this] { cursor_.in_window->SelectAll(); }});

    // naviagtion
    MGO_KEYMAP("<left>", {[this] { cursor_.in_window->CursorGoLeft(); }});
    MGO_KEYMAP("<right>", {[this] { cursor_.in_window->CursorGoRight(); }});
    MGO_KEYMAP("<c-left>",
               {[this] { cursor_.in_window->CursorGoWordBegin(); }});
    MGO_KEYMAP("<c-right>",
               {[this] { cursor_.in_window->CursorGoWordEnd(true); }});
    MGO_KEYMAP("<up>", {[this] { CursorUp(); }}, kAllModes);
    MGO_KEYMAP("<down>", {[this] { CursorDown(); }}, kAllModes);
    MGO_KEYMAP("<c-p>", {[this] { CursorUp(); }}, kAllModes);
    MGO_KEYMAP("<c-n>", {[this] { CursorDown(); }}, kAllModes);
    MGO_KEYMAP("<home>", {[this] { cursor_.in_window->CursorGoHome(); }});
    MGO_KEYMAP("<end>", {[this] { cursor_.in_window->CursorGoEnd(); }});
    MGO_KEYMAP(
        "<pgdn>", {[this] {
            cursor_.in_window->CursorGoDown(cursor_.in_window->frame_.height_);
        }});
    MGO_KEYMAP(
        "<pgup>", {[this] {
            cursor_.in_window->CursorGoUp(cursor_.in_window->frame_.height_);
        }});
    MGO_KEYMAP("<c-g>", {[this] {
                   GotoPeel();
                   peel_->AddStringAtCursor("g ");
               }});
    MGO_KEYMAP("<esc>", {[this] {
                   cursor_.in_window->frame_.selection_.active = false;
               }});
}

void Editor::InitKeymapsVi() {}

#define MGO_CMD command_manager_.AddCommand

void Editor::InitCommands() {
    MGO_CMD({"h",
             "",
             {},
             [this](CommandArgs args) {
                 (void)args;
                 Help();
             },
             0});
    MGO_CMD({"e",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 const std::string& name_str = std::get<std::string>(args[0]);
                 Buffer* b = buffer_manager_.FindBuffer(name_str);
                 if (b) {
                     cursor_.in_window->AttachBuffer(b);
                     return;
                 }
                 cursor_.in_window->AttachBuffer(buffer_manager_.AddBuffer(
                     Buffer(global_opts_.get(), Path(name_str))));
             },
             1});
    MGO_CMD({"b",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 const std::string& name_str = std::get<std::string>(args[0]);
                 Buffer* b = buffer_manager_.FindBuffer(name_str);
                 if (b) {
                     cursor_.in_window->AttachBuffer(b);
                 }
             },
             1});
    MGO_CMD({"s",
             "",
             {Type::kString},
             [this](CommandArgs args) {
                 MGO_LOG_DEBUG("search %s",
                               std::get<std::string>(args[0]).c_str());
                 cursor_.in_window->BuildSearchContext(
                     std::get<std::string>(args[0]));
                 SearchNext();
             },
             1});
    MGO_CMD({"g",
             "",
             {Type::kInteger},
             [this](CommandArgs args) {
                 cursor_.in_window->CursorGoLine(std::get<int64_t>(args[0]));
             },
             1});
}

void Editor::InitCommandsVi() {}

void Editor::PrintKey(const Terminal::KeyInfo& key_info) {
    bool ctrl = key_info.mod & Terminal::Mod::kCtrl;
    bool shift = key_info.mod & Terminal::Mod::kShift;
    bool alt = key_info.mod & Terminal::Mod::kAlt;
    bool motion = key_info.mod & Terminal::Mod::kMotion;
    char c[5];
    int len = UnicodeToUtf8(key_info.codepoint, c);
    c[len] = '\0';
    MGO_LOG_DEBUG(
        "ctrl %d shift %d alt %d motion %d special key %d codepoint "
        "\\U%08" PRIx32 " char %s",
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
    Result res = kKeyseqError;
    Keyseq* handler;
    if (key_info.IsSpecialKey() || key_info.codepoint <= CHAR_MAX) {
        res = keymap_manager_.FeedKey(key_info, handler);
    }
    if (res == kKeyseqError) {
        // Pure codepoints not handled by the keymap manager.
        // Use single codepoint to edit buffers is quite safe here.

        // We may use a codepoint as grapheme when we meet some ascii
        // characters, like '(', '[' '{', because they're very very rare as a
        // part of multi-codepoint graphemes.
        if (!key_info.IsSpecialKey()) {
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
            if (res == kOk) {
                StartAutoCompletionTimer();
            }
        }
        return;
    } else if (res == kKeyseqMatched) {
        // MGO_LOG_DEBUG("keymap matched");
        return;
    }

    handler->f();
}

void Editor::HandleLeftClick(int s_row, int s_col) {
    Window* win = LocateWindow(s_col, s_row);
    Window* prev_win = cursor_.in_window;
    Pos prev_pos = {cursor_.line, cursor_.byte_offset};
    if (win) {
        cursor_.in_window = win;
        win->SetCursorHint(s_row, s_col);
    }

    if (mouse_.state == MouseState::kReleased) {
        if (!win) {
            return;
        }
        if (cursor_.in_window->frame_.selection_.active) {
            cursor_.in_window->frame_.selection_.active = false;
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
            if (win == prev_win &&
                !(prev_pos == Pos{cursor_.line, cursor_.byte_offset})) {
                cursor_.in_window->frame_.selection_.active = true;
                cursor_.in_window->frame_.selection_.anchor = prev_pos;
                cursor_.in_window->frame_.selection_.head = {
                    cursor_.line, cursor_.byte_offset};
            }
        }
    } else if (mouse_.state == MouseState::kLeftHolding) {
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
            cursor_.in_window->frame_.selection_.active = false;
            break;
        }
        case mk::kMiddle: {
            mouse_.state = MouseState::kMiddleNotReleased;
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

void Editor::HandleResize() {
    Terminal::ResizeInfo resize_info = term_.EventResizeInfo();
    Resize(resize_info.width, resize_info.height);
}

void Editor::Draw() {
    // TODO: do not redraw not modified part

    // First clear the screen so we don't need to print spaces for blank
    // screen parts
    term_.Clear();

    window_->Draw();
    status_line_->Draw();
    peel_->Draw();

    // Put it at last so it can override some parts
    cmp_menu_->Draw();

    // Draw cursor
    if (cursor_.s_col == -1 && cursor_.s_row == -1) {
        // when not make visible
        term_.HideCursor();
    } else if (cursor_.s_row != cursor_.s_row_last ||
               cursor_.s_col != cursor_.s_col_last) {
        term_.SetCursor(cursor_.s_col, cursor_.s_row);
        if (cursor_.blinking_timer_) {
            loop_->timer_manager_.StartTimer(cursor_.blinking_timer_.get());
            cursor_.show = true;
        }
    } else if (cursor_.show) {
        term_.SetCursor(cursor_.s_col, cursor_.s_row);
    } else {
        term_.HideCursor();
    }

    term_.Present();
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    if (window_->frame_.buffer_->state() == BufferState::kHaveNotRead) {
        try {
            window_->frame_.buffer_->Load();
            // TODO: Not init if file is too big.
            syntax_parser_->SyntaxInit(window_->frame_.buffer_);
        } catch (Exception& e) {
            MGO_LOG_ERROR("buffer %s : %s",
                          zstring_view_c_str(window_->frame_.buffer_->Name()),
                          e.what());
            // TODO: Maybe Notify the user
        }
    }

    window_->frame_.MakeSureViewValid();
    // Peel no need valid because it only have one line.
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
    Buffer* b = buffer_manager_.FindBuffer(Path::GetAppRoot() + kHelpPath);
    if (b) {
        cursor_.in_window->AttachBuffer(b);
        return;
    }
    b = buffer_manager_.AddBuffer(Buffer(global_opts_.get(), p, true));
    cursor_.in_window->AttachBuffer(b);
}

void Editor::Quit() {
    MGO_ASSERT(buffer_manager_.Begin());
    bool have_not_saved = false;
    for (auto buffer = buffer_manager_.Begin(); buffer != buffer_manager_.End();
         buffer = buffer->next_) {
        if (buffer->state() == BufferState::kModified) {
            have_not_saved = true;
            break;
        }
    }
    if (!have_not_saved) {
        loop_->EndLoop();
        return;
    }

    ask_quit_ = true;
    if (IsPeel(mode_)) {
        peel_->SetContent("");
        cursor_.byte_offset = 0;
    } else {
        GotoPeel();
    }
    peel_->AddStringAtCursor(kAskQuitStr);
}

void Editor::GotoPeel() {
    MGO_ASSERT(!IsPeel(mode_));

    peel_->SetContent("");
    cursor_.in_window->frame_.b_view_->SaveCursorState(&cursor_);
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
        const Frame& f = cursor_.in_window->frame_;
        f.b_view_->RestoreCursorState(&cursor_, f.buffer_);
    }
    mode_ = Mode::kEdit;
}

void Editor::ExitFromModeVi() {
    if (IsPeel(mode_)) {
        MGO_ASSERT(cursor_.restore_from_peel);
        cursor_.in_window = cursor_.restore_from_peel;
        const Frame& f = cursor_.in_window->frame_;
        f.b_view_->RestoreCursorState(&cursor_, f.buffer_);
    }
    mode_ = Mode::kViNormal;
}

void Editor::SearchNext() {
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

void Editor::TriggerCompletion(bool autocmp) {
    if (CompletionTriggered()) {
        CancellCompletion();
    }

    bool in_peel = IsPeel(mode_);
    if (!in_peel) {
        tmp_completer_ = window_->frame_.buffer_->completer();
    } else {
        tmp_completer_ = &peel_->completer_;
    }

    if (tmp_completer_ == nullptr) {
        if (autocmp) {
            return;
        }
        if (!in_peel) {
            peel_->SetContent("No completion source");
        }
        return;
    }

    std::vector<std::string> entries;
    tmp_completer_->Suggest(cursor_.ToPos(), entries);
    if (entries.empty()) {
        tmp_completer_->Cancel();
        if (!autocmp && !in_peel) {
            peel_->SetContent("No completion");
        }
        tmp_completer_ = nullptr;
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

    tmp_completer_->Cancel();
    tmp_completer_ = nullptr;
    cmp_menu_->Clear();
    cmp_menu_->visible() = false;
}

bool Editor::CompletionTriggered() { return tmp_completer_ != nullptr; }

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

void Editor::PeelHitEnter() {
    if (ask_quit_) {
        std::string content = peel_->GetContent();
        ask_quit_ = false;
        if (content.empty()) {
            ExitFromMode();
            return;
        }
        if (content.back() == 'y') {
            loop_->EndLoop();
        }
        ExitFromMode();
        return;
    }

    if (CompletionTriggered()) {
        if (tmp_completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
            kRetriggerCmp) {
            tmp_completer_ = nullptr;
            TriggerCompletion(true);
        } else {
            tmp_completer_ = nullptr;
        }
        return;
    }

    CommandArgs args;
    Command* c;
    Result res = command_manager_.EvalCommand(peel_->GetContent(), args, c);
    if (res == kCommandEmpty) {
        ExitFromMode();
        return;
    }

    if (res != kOk) {
        peel_->SetContent("Wrong Command");
        ExitFromMode();
        return;
    }

    ExitFromMode();
    c->f(args);
}

void Editor::CursorUp() {
    if (CompletionTriggered()) {
        cmp_menu_->SelectPrev();
        show_cmp_menu_ = true;
    } else if (!IsPeel(mode_)) {
        cursor_.in_window->CursorGoUp(count_);
    }
}

void Editor::CursorDown() {
    if (CompletionTriggered()) {
        cmp_menu_->SelectNext();
        show_cmp_menu_ = true;
    } else if (!IsPeel(mode_)) {
        cursor_.in_window->CursorGoDown(count_);
    }
}

void Editor::RemoveCurrentBuffer() {
    buffer_manager_.RemoveBuffer(cursor_.in_window->frame_.buffer_);
}

void Editor::SaveCurrentBuffer() {
    try {
        Result res = cursor_.in_window->frame_.buffer_->Write();
        if (res == kBufferNoBackupFile) {
            peel_->SetContent("Buffer no backup file");
        } else if (res == kBufferCannotLoad) {
            peel_->SetContent("Buffer can't load");
        } else if (res == kBufferReadOnly) {
            peel_->SetContent("Buffer read only");
        }
    } catch (IOException& e) {
        MGO_LOG_ERROR("%s", e.what());
        std::stringstream ss;
        ss << "Buffer can't save: " << e.what();
        peel_->SetContent(ss.str());
    }
}

void Editor::StartAutoCompletionTimer() {
    // We start a timer, every terminal event will cancel it.
    // So if the timer is timeout, user input seems over, but we shouldn't rely
    // on that and think the current cursor pos is a real grapheme
    // end(user-percieved). Good time to trigger a autocmp.
    loop_->timer_manager_.StartTimer(autocmp_trigger_timer_.get());
}

}  // namespace mango