#include "editor.h"

#include "coding.h"
#include "options.h"
#include "term.h"

namespace mango {

void Editor::Loop(std::unique_ptr<Options> options) {
    options_ = std::move(options);
    status_line_ = std::make_unique<StatusLine>(&cursor_, options_.get());
    peel_ = std::make_unique<MangoPeel>(&cursor_, options_.get());

    // Create all buffers
    for (const char* path : options_->begin_files) {
        auto buffer = Buffer(path);
        int64_t buffer_id = buffer.id();
        auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
        assert(inserted);
        iter->second.AppendToList(buffer_list_tail_);
    }

    // Create the first window.
    // If no buffer then create one.
    Buffer* buf;
    if (buffer_list_head_.next_ != nullptr) {
        buf = buffer_list_head_.next_;
    } else {
        Buffer buffer;
        int64_t buffer_id = buffer.id();
        auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
        assert(inserted);
        iter->second.AppendToList(buffer_list_tail_);
        buf = &iter->second;
    }
    auto win = new Window(buf, &cursor_, options_.get());
    win->AppendToList(window_list_tail_);

    // Set Cursor in the first window
    cursor_.in_window = window_list_head_.next_;

    // manually trigger resizing to create the layout
    Resize(term_.Width(), term_.Height());

    InitKeymaps();

    // Event Loop
    while (!quit_) {
        PreProcess();
        Draw();

        // Poll a new Event
        if (!term_.Poll(options_->poll_event_timeout_ms)) {
            continue;
        }

        // Handle it and do sth
        if (term_.EventIsKey()) {
            HandleKey();
        } else if (term_.EventIsMouse()) {
            HandleMouse();
        } else if (term_.EventIsResize()) {
            HandleResize();
        } else {
            assert(false);
        }
    }
}

void Editor::InitKeymaps() {
    std::vector<Mode> efp = {Mode::kEdit, Mode::kFind, Mode::kPeelCommand};
    keymap_manager_.AddKeymap("<c-b><c-n>",
                              {[this] { cursor_.in_window->NextBuffer(); }});
    keymap_manager_.AddKeymap("<c-b><c-p>",
                              {[this] { cursor_.in_window->PrevBuffer(); }});
    keymap_manager_.AddKeymap("<c-pgdn>",
                              {[this] { cursor_.in_window->NextBuffer(); }});
    keymap_manager_.AddKeymap("<c-pgup>",
                              {[this] { cursor_.in_window->PrevBuffer(); }});
    keymap_manager_.AddKeymap("<c-p>", {[this] {}});
    keymap_manager_.AddKeymap("<c-q>", {[this] { Quit(); }});
    keymap_manager_.AddKeymap(
        "<c-s>", {[this] {
            try {
                Result res = cursor_.in_window->frame_.buffer_->WriteAll();
                if (res == kBufferNoBackupFile) {
                    // TODO: notify user
                } else if (res == kBufferCannotRead) {
                    // TODO: notify user
                }
            } catch (IOException& e) {
                MANGO_LOG_ERROR("%s", e.what());
                // TODO: notify user
            }
        }});
    keymap_manager_.AddKeymap(
        "<bs>", {[this] { cursor_.in_window->DeleteCharacterBeforeCursor(); }},
        kDefaultsModes);
    keymap_manager_.AddKeymap(
        "<bs>", {[this] { peel_->DeleteCharacterBeforeCursor(); }},
        {Mode::kPeelCommand});
    keymap_manager_.AddKeymap("<tab>",
                              {[this] { cursor_.in_window->TabAtCursor(); }});
    keymap_manager_.AddKeymap("<tab>", {[this] { peel_->TabAtCursor(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeymap(
        "<enter>",
        {[this] { cursor_.in_window->AddStringAtCursor(kNewLine); }});
    keymap_manager_.AddKeymap("<enter>", {[this] {  // TODO
                              }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeymap("<enter>", {[this] {
                                  // TODO
                              }},
                              {Mode::kPeelShow});
    keymap_manager_.AddKeymap("<left>",
                              {[this] { cursor_.in_window->CursorGoLeft(); }});
    keymap_manager_.AddKeymap("<left>", {[this] { peel_->CursorGoLeft(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeymap("<right>",
                              {[this] { cursor_.in_window->CursorGoRight(); }});
    keymap_manager_.AddKeymap("<right>", {[this] { peel_->CursorGoRight(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeymap("<up>",
                              {[this] { cursor_.in_window->CursorGoUp(); }});
    keymap_manager_.AddKeymap("<down>",
                              {[this] { cursor_.in_window->CursorGoDown(); }});
    keymap_manager_.AddKeymap("<home>",
                              {[this] { cursor_.in_window->CursorGoHome(); }});
    keymap_manager_.AddKeymap("<home>", {[this] { peel_->CursorGoHome(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeymap("<end>",
                              {[this] { cursor_.in_window->CursorGoEnd(); }});
    keymap_manager_.AddKeymap("<end>", {[this] { peel_->CursorGoEnd(); }},
                              {Mode::kPeelCommand});
    keymap_manager_.AddKeymap("<pgdn>", {[this] {
                                  cursor_.in_window->ScrollRows(
                                      cursor_.in_window->frame_.height_ - 1);
                              }});
    keymap_manager_.AddKeymap("<pgup>", {[this] {
                                  cursor_.in_window->ScrollRows(
                                      -cursor_.in_window->frame_.height_ - 1);
                              }});
}

void Editor::HandleKey() {
    Terminal::KeyInfo key_info = term_.EventKeyInfo();
    bool ctrl = key_info.mod & Terminal::Mod::kCtrl;
    bool shift = key_info.mod & Terminal::Mod::kShift;
    bool alt = key_info.mod & Terminal::Mod::kAlt;
    bool motion = key_info.mod & Terminal::Mod::kMotion;
    (void)ctrl, (void)shift, (void)alt, (void)motion;

    KeymapHandler* handler;
    Result res = keymap_manager_.FeedKey(key_info, handler);
    if (res == kKeymapError) {
        // pure characters
        // not handled by the keymap manager
        if (!key_info.IsSpecialKey() && key_info.mod == 0) {
            uint32_t codepoint = key_info.codepoint;
            char c[6];
            int len = UnicodeToUtf8(codepoint, c);
            assert(len > 0);
            cursor_.in_window->AddStringAtCursor(c);
        }
        return;
    } else if (res == kKeymapMatched) {
        MANGO_LOG_DEBUG("keymap matched");
        return;
    }

    // Match Done
    handler->f();
}

void Editor::HandleMouse() {
    Terminal::MouseInfo mouse_info = term_.EventMouseInfo();

    switch (mouse_info.t) {
        using mk = Terminal::MouseKey;
        case mk::kLeft: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                cursor_.in_window = win;
                win->SetCursorHint(mouse_info.row, mouse_info.col);
            }
            // Not locate any area, do nothing
            break;
        }
        case mk::kRight: {
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
        case mk::kRelease: {
            break;
        }
        case mk::kMiddle: {
            break;
        }
    }
}

// TODO: support multi window
void Editor::Resize(int width, int height) {
    Window* first_win = window_list_head_.next_;
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

void Editor::Quit() { quit_ = true; }

void Editor::Draw() {
    // First clear the screen so we don't need to print spaces for blank
    // screen parts
    // TODO: do not redraw not modified part
    term_.Clear();
    Result ret = term_.SetCursor(cursor_.s_col, cursor_.s_row);
    if (ret == kTermOutOfBounds) {
        return;
    }

    for (Window* window = window_list_head_.next_; window != nullptr;
         window = window->next_) {
        if (window->frame_.buffer_->IsReadAll()) {
            window->Draw();
        }
    }

    status_line_->Draw();

    peel_->frame_.Draw();

    term_.Present();
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    for (Window* window = window_list_head_.next_; window != nullptr;
         window = window->next_) {
        if (window->frame_.buffer_->state() == BufferState::kHaveNotRead) {
            try {
                window->frame_.buffer_->ReadAll();
            } catch (FileCreateException& e) {
                window->frame_.buffer_->state() = BufferState::kCannotCreate;
                MANGO_LOG_ERROR("%s", e.what());
                // TODO: Notify the user
            } catch (IOException& e) {
                window->frame_.buffer_->state() = BufferState::kCannotRead;
                MANGO_LOG_ERROR("%s", e.what());
                // TODO: Notify the user
            }
        }
    }
    if (mode_ == Mode::kEdit || mode_ == Mode::kFind) {
        cursor_.in_window->MakeCursorVisible();
    } else if (mode_ == Mode::kPeelCommand) {
        peel_->MakeCursorVisible();
    }
}

Window* Editor::LocateWindow(int s_col, int s_row) {
    for (Window* win = window_list_head_.next_; win != nullptr;
         win = win->next_) {
        if (win->frame_.In(s_col, s_row)) {
            return win;
        }
    }
    return nullptr;
}

Editor& Editor::GetInstance() {
    static Editor editor;
    return editor;
}
}  // namespace mango