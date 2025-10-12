#pragma once

#include <memory>
#include <unordered_map>

#include "buffer.h"
#include "cursor.h"
#include "options.h"
#include "state.h"
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
    void Loop(Options* options);

    void HandleKey();
    void HandleMouse();
    void HandleResize();

    void Draw();
    void PreProcess();

    void Quit();

    static Editor& GetInstance() {
        static Editor editor;
        return editor;
    }

   private:
    std::unordered_map<int64_t, Buffer> buffers_;
    // Now only support one window in the screen
    // TODO: mutiple window logic
    std::unordered_map<int64_t, std::unique_ptr<Window>> windows_;
    int64_t buffer_id_cur_ = 0;
    int64_t window_id_cur_ = 0;
    bool quit_ = false;

    Cursor cursor_;

    Options* options_ = nullptr;

    MouseState moust_state_ = MouseState::released;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
