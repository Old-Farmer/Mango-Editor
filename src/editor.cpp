#include "editor.h"

#include "coding.h"
#include "term.h"

namespace mango {

void Editor::Loop(std::unique_ptr<Options> options) {
    options_ = std::move(options);
    status_line_ = std::make_unique<StatusLine>(&cursor_, options_.get());

    // Create all buffers
    for (const char* path : options_->begin_files) {
        auto buffer = Buffer(path);
        int64_t buffer_id = buffer.id();
        auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
        assert(inserted);
    }

    // Create the first window
    Buffer* buf;
    if (!buffers_.empty()) {
        buf = &buffers_.begin()->second;
    } else {
        Buffer buffer;
        int64_t buffer_id = buffer.id();
        auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
        assert(inserted);
        buf = &iter->second;
    }
    std::unique_ptr<Window> win =
        std::make_unique<Window>(buf, &cursor_, options_.get());
    int64_t win_id = win->id();
    windows_.emplace(win_id, std::move(win));

    // Set Cursor in the first window
    cursor_.in_window = windows_.begin()->second.get();

    // manually trigger resizing to create the layout
    Resize(term_.Width(), term_.Height());

    // Event Loop
    while (!quit_) {
        PreProcess();
        Draw();

        // Show
        term_.Present();

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

void Editor::HandleKey() {
    Terminal::KeyInfo key_info = term_.EventKeyInfo();
    bool ctrl = key_info.mod & Terminal::Mod::kCtrl;
    bool shift = key_info.mod & Terminal::Mod::kShift;
    bool alt = key_info.mod & Terminal::Mod::kAlt;
    bool motion = key_info.mod & Terminal::Mod::kMotion;
    if (key_info.IsSpecialKey()) {
        switch (key_info.special_key) {
            using sk = Terminal::SpecialKey;
            case sk::kCtrlQ: {
                if (ctrl && !shift && !alt && !motion) {
                    Quit();
                }
                break;
            }
            case sk::kCtrlS: {
                try {
                    Result res = cursor_.in_window->buffer_->WriteAll();
                    if (res == kBufferNoBackupFile) {
                        // TODO: notify user
                    }
                } catch (IOException& e) {
                    MANGO_LOG_ERROR("%s", e.what());
                    // TODO: notify user
                }
                break;
            }
            case sk::kBackspace2: {
                if (ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->DeleteCharacterBeforeCursor();
                }
                break;
            }
            case sk::kEnter: {
                if (ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->AddCharacterAtCursor(kNewLine);
                }
                break;
            }
            case sk::kArrowLeft: {
                if (!ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->CursorGoLeft();
                }
                break;
            }
            case sk::kArrowRight: {
                if (!ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->CursorGoRight();
                }
                break;
            }
            case sk::kArrowUp: {
                if (!ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->CursorGoUp();
                }
                break;
            }
            case sk::kArrowDown: {
                if (!ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->CursorGoDown();
                }
                break;
            }
            case sk::kHome: {
                if (!ctrl && !shift && !alt && !motion) {
                    MANGO_LOG_DEBUG("home");
                    cursor_.in_window->CursorGoHome();
                }
                break;
            }
            case sk::kEnd: {
                if (!ctrl && !shift && !alt && !motion) {
                    cursor_.in_window->CursorGoEnd();
                }
                break;
            }
            default:
                return;
        }
    } else {
        // pure characters
        if (!ctrl && !shift && !alt && !motion) {
            uint32_t codepoint = key_info.codepoint;
            char c[6];
            int len = UnicodeToUtf8(codepoint, c);
            assert(len > 0);
            cursor_.in_window->AddCharacterAtCursor(c);
        }
    }
}

void Editor::HandleMouse() {
    Terminal::MouseInfo mouse_info = term_.EventMouseInfo();

    switch (mouse_info.t) {
        using mk = Terminal::MouseKey;
        case mk::kLeft: {
            // TODO: multiple window
            cursor_.in_window->SetCursorHint(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kRight: {
            break;
        }
        case mk::kWheelDown: {
            // TODO: multiple window
            cursor_.in_window->ScrollRows(
                options_->scroll_rows_per_mouse_wheel_scroll);
            break;
        }
        case mk::kWheelUp: {
            // TODO: multiple window
            cursor_.in_window->ScrollRows(
                -options_->scroll_rows_per_mouse_wheel_scroll);
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
    Window* first_win = windows_.begin()->second.get();
    first_win->col_ = 0;
    first_win->row_ = 0;
    first_win->width_ = width;
    first_win->height_ = height - 1;

    status_line_->row_ = height - 1;
    status_line_->width_ = width;
}

void Editor::HandleResize() {
    Terminal::ResizeInfo resize_info = term_.EventResizeInfo();
    Resize(resize_info.width, resize_info.height);
}

void Editor::Quit() { quit_ = true; }

void Editor::Draw() {
    // First clear the screen so we don't need to print spaces for blank screen
    // parts
    // TODO: do not redraw not modified part
    term_.Clear();
    Result ret = term_.SetCursor(cursor_.s_col, cursor_.s_row);
    if (ret == kTermOutOfBounds) {
        return;
    }

    for (auto& [_, window] : windows_) {
        if (window->buffer_->IsReadAll()) {
            window->Draw();
        }
    }

    status_line_->Draw();
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    for (auto& [_, window] : windows_) {
        if (window->buffer_->state() == BufferState::kHaveNotRead) {
            try {
                window->buffer_->ReadAll();
            } catch (FileCreateException& e) {
                window->buffer_->state() = BufferState::kCannotCreate;
                MANGO_LOG_ERROR("%s", e.what());
                // TODO: Notify the user
            } catch (IOException& e) {
                window->buffer_->state() = BufferState::kCannotRead;
                MANGO_LOG_ERROR("%s", e.what());
                // TODO: Notify the user
            }
        }
    }
    cursor_.in_window->MakeCursorVisible();
}

Editor& Editor::GetInstance() {
    static Editor editor;
    return editor;
}

Options* Editor::GetOption() { return options_.get(); }

std::unique_ptr<Options> Editor::options_ = nullptr;

}  // namespace mango