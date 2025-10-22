#pragma once

#include <memory>

#include "buffer.h"
#include "buffer_manager.h"
#include "cmp_menu.h"
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

    void GotoPeel();
    void ExitFromMode();
    void TriggerCmp(std::function<void(size_t)> accept_call_back,
                    std::function<void(void)> cancel_call_back,
                    std::vector<std::string> entries);
    void SearchNext();
    void SearchPrev();

   private:
    void InitKeymaps();
    void InitCommands();
    void HandleKey();
    void HandleMouse();
    void HandleResize();

    void Draw();
    void PreProcess();
    void Resize(int width, int height);

    // helper methods
    Window* LocateWindow(int s_col, int s_row);

   private:
    MouseState moust_state_ = MouseState::kReleased;
    Mode mode_ = Mode::kEdit;

    BufferManager buffer_manager_;
    KeymapManager keymap_manager_{mode_};
    CommandManager command_manager;

    Cursor cursor_;
    // Now only support one window in the screen
    // TODO: mutiple window logic
    std::unique_ptr<Window> window_;
    std::unique_ptr<StatusLine> status_line_;
    std::unique_ptr<MangoPeel> peel_;
    std::unique_ptr<CmpMenu> cmp_menu_;

    bool quit_ = false;

    std::unique_ptr<Options> options_;

    // Cmp context
    std::function<void(size_t)> cmp_accept_callback_ = nullptr;
    std::function<void()> cmp_cancel_callback_ = nullptr;
    Mode mode_trigger_cmp_;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
