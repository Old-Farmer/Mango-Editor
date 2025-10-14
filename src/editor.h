#pragma once

#include <memory>
#include <unordered_map>

#include "buffer.h"
#include "cursor.h"
#include "options.h"
#include "state.h"
#include "status_line.h"
#include "utils.h"
#include "window.h"

namespace mango {

class Editor {
   private:
    MANGO_DEFAULT_CONSTRUCT_DESTRUCT(Editor);

   public:
    MANGO_DELETE_COPY(Editor);
    MANGO_DELETE_MOVE(Editor);

    // Make sure that options is static lifetime
    // throws TermException
    void Loop(std::unique_ptr<Options> options);

    void HandleKey();
    void HandleMouse();
    void HandleResize();

    void Draw();
    void PreProcess();

    void Quit();

    void Resize(int width, int height);

    static Editor& GetInstance();

    static Options* GetOption();

   private:
    std::unordered_map<int64_t, Buffer> buffers_;
    // Now only support one window in the screen
    // TODO: mutiple window logic
    Window window_list_head_ = Window::CreateListHead();
    Window* window_list_tail_ = &window_list_head_;

    bool quit_ = false;

    Cursor cursor_;
    std::unique_ptr<StatusLine> status_line_;

    static std::unique_ptr<Options> options_;

    MouseState moust_state_ = MouseState::kReleased;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
