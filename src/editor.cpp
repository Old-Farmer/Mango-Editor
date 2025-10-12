#include "editor.h"

#include "coding.h"
#include "term.h"

namespace mango {

void Editor::Loop(Options* options) {
    options_ = options;

    {
        auto first_win = std::make_unique<Window>();
        int64_t id = first_win->id();
        windows_.emplace(id, std::move(first_win));
    }

    auto window_iter = windows_.begin();
    Window* first_win = window_iter->second.get();
    first_win->height_ = term_.Height();
    first_win->width_ = term_.Width();
    if (!options_->begin_files.empty()) {
        try {
            // TODO: support multiple file
            auto buffer = Buffer(options_->begin_files[0]);
            int64_t id = buffer.id();
            auto [iter, inserted] = buffers_.emplace(id, std::move(buffer));
            assert(inserted);
            first_win->buffer_ = &(iter->second);
        } catch (IOException& e) {
            // TODO: Notify the user
            ;
        }
    }
    first_win->cursor_ = &cursor_;

    // Set Cursor
    cursor_.in_window = first_win;

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

// TODO: support Mouse
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
void Editor::HandleResize() {
    Window* first_win = windows_.begin()->second.get();

    Terminal::ResizeInfo resize_info = term_.EventResizeInfo();
    first_win->width_ = resize_info.width;
    first_win->height_ = resize_info.height;
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
        if (window->buffer_ != nullptr && window->buffer_->IsReadAll()) {
            window->Draw();
        }
    }
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    for (auto& [_, window] : windows_) {
        if (window->buffer_ != nullptr && !window->buffer_->IsReadAll()) {
            try {
                window->buffer_->ReadAll();
            } catch (IOException& e) {
                MANGO_LOG_ERROR("%s", e.what());
                // TODO: Notify the user
            }
        }
    }
    cursor_.in_window->MakeCursorVisible();
}

}  // namespace mango