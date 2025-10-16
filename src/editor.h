#pragma once

#include <memory>
#include <unordered_map>

#include "buffer.h"
#include "cursor.h"
#include "keymap_manager.h"
#include "mango_peel.h"
#include "state.h"
#include "status_line.h"
#include "utils.h"
#include "window.h"

namespace mango {

class Options;

class Editor {
   private:
    MANGO_DEFAULT_CONSTRUCT_DESTRUCT(Editor);

   public:
    MANGO_DELETE_COPY(Editor);
    MANGO_DELETE_MOVE(Editor);

    // Make sure that options is static lifetime
    // throws TermException
    void Loop(std::unique_ptr<Options> options);

    void InitKeymaps();
    void HandleKey();
    void HandleMouse();
    void HandleResize();

    void Draw();
    void PreProcess();

    void Quit();

    void Resize(int width, int height);

    Window* LocateWindow(int s_col, int s_row);

    static Editor& GetInstance();

   private:
    MouseState moust_state_ = MouseState::kReleased;
    Mode mode_ = Mode::kEdit;

    // Buffers
    std::unordered_map<int64_t, Buffer> buffers_;
    Buffer buffer_list_head_ = Buffer();
    Buffer* buffer_list_tail_ = &buffer_list_head_;

    // Now only support one window in the screen
    // TODO: mutiple window logic
    Window window_list_head_ = Window::CreateListHead();
    Window* window_list_tail_ = &window_list_head_;

    bool quit_ = false;

    Cursor cursor_;
    std::unique_ptr<Options> options_;

    std::unique_ptr<MangoPeel> peel_;
    std::unique_ptr<StatusLine> status_line_;

    KeymapManager keymap_manager_{mode_};

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
