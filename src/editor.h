#pragma once

#include <memory>

#include "buffer.h"
#include "buffer_manager.h"
#include "command_manager.h"
#include "cursor.h"
#include "keymap_manager.h"
#include "mango_peel.h"
#include "state.h"
#include "status_line.h"
#include "utils.h"
#include "window.h"

namespace mango {

class InitOptions;
class Options;

class Editor {
   private:
    MANGO_DEFAULT_CONSTRUCT_DESTRUCT(Editor);

   public:
    MANGO_DELETE_COPY(Editor);
    MANGO_DELETE_MOVE(Editor);

    // Make sure that options is static lifetime
    // throws TermException
    void Loop(std::unique_ptr<Options> options,
              std::unique_ptr<InitOptions> init_options);

    static Editor& GetInstance();

    void Help();
    void Quit();

   private:
    void InitKeymaps();
    void InitCommands();
    void HandleKey();
    void HandleMouse();
    void HandleResize();

    void Draw();
    void PreProcess();
    void Resize(int width, int height);

    void GotoPeel();
    void ExitFromMode();
    void SearchNext();
    void SearchPrev();

    // helper methods
    Window* LocateWindow(int s_col, int s_row);

   private:
    MouseState moust_state_ = MouseState::kReleased;
    Mode mode_ = Mode::kEdit;

    BufferManager buffer_manager_;

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
    CommandManager command_manager;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
